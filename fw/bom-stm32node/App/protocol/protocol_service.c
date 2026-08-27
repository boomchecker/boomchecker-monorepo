/**
 ******************************************************************************
 * @file    protocol_service.c
 ******************************************************************************
 */
#include "protocol_service.h"

#include <stdio.h>

#include "boomlink_codec.h"
#include "boomlink_command_service.h"
#include "boomlink_config_store.h"
#include "boomlink_dispatch.h"
#include "boomlink_flash_storage_port.h"
#include "link_service.h"
#include "main.h" /* HAL_GetTick, HAL_NVIC_SystemReset */
#include "radio.h"

/* See boomlink_command_service.h's reboot() doc: a synchronous
   HAL_NVIC_SystemReset() inside the command callback would reset this node
   before its own CommandResponse - the one this exact reboot request is
   waiting on - ever reaches the radio. This is how long cmd_reboot() below
   instead makes the caller wait: long enough for a just-queued HIGH-
   priority response (see protocol_service_on_rx()'s priority choice) to
   actually clear the TX pipeline under the bring-up LoRa profile's airtime
   plus a superloop iteration or two of scheduling slack. Not a measured
   value - the same "reasonable by inspection, not yet tuned" caveat this
   codebase's other bring-up-only constants carry (e.g. link_service.c's
   LINK_* macros) - but generous enough that undershooting it would need the
   superloop to stall for half a second, which would already be a problem
   far bigger than a late reboot. */
#define PROTOCOL_SERVICE_REBOOT_DELAY_MS 500u

/* Section 8.2's revert-on-timeout window for a hazardous config change
   (node_id/magic/radio) - boomlink_config_service_init()'s own doc leaves
   this policy choice to its caller rather than picking one itself. 5
   seconds: comfortably longer than this node's own PENDING_CONFIRMATION
   response and a peer's next request both actually happening at LoRa
   bring-up speeds (a few hundred ms of airtime plus retry/backoff margin -
   see link_service.c's LINK_* constants), short enough that a change
   nobody ever confirms does not leave the node's config half-changed for
   long. Not tuned against real deployment traffic, the same caveat as
   PROTOCOL_SERVICE_REBOOT_DELAY_MS above. */
#define PROTOCOL_SERVICE_CONFIRM_WINDOW_MS 5000u

static boomlink_dispatch_t       s_dispatch;
static boomlink_config_service_t s_config_svc;
static boomlink_command_ops_t    s_command_ops;
static bool                      s_initialized;

/* Initialized once by protocol_service_load_config() (always called before
   anything else in this file can run - see cli.c's boot sequence) and
   reused by persist_current_config() below for every later save, rather
   than re-initializing a local one per call: boomlink_flash_storage_port_
   init()'s own doc says it is safe to call at any point in boot, but
   nothing about it needing to be called more than once, and a single
   stable instance is what App/link/link_service.c's own s_port already
   does for the radio port seam. */
static boomlink_storage_port_t s_storage_port;

static bool     s_reboot_armed;
static uint32_t s_reboot_armed_at_ms;

/* See protocol_service_on_rx()'s long comment on the commit/confirm design
   for what this guards against. */
static bool s_confirm_eligible;

/* boomlink_command_ops_t.reboot - arms a deferred reset for protocol_
   service_process() to perform once PROTOCOL_SERVICE_REBOOT_DELAY_MS has
   elapsed, rather than resetting here - see this file's macro doc and
   boomlink_command_service.h's own doc for why a synchronous reset here
   would be wrong. Always succeeds: arming a flag cannot fail the way a real
   hardware action could. */
static bool cmd_reboot(void *ctx) {
  (void)ctx;
  s_reboot_armed       = true;
  s_reboot_armed_at_ms = HAL_GetTick();
  return true;
}

/* boomlink_command_ops_t.self_test - the only hardware this bring-up
   firmware has anything to test is the radio (App/radio/radio.h); no
   detection algorithm or other sensor exists yet to include here (see
   boomlink.md's Phase C notes on what this PR deliberately does not add). */
static bool cmd_self_test(void *ctx, char *out_diagnostic, size_t out_diagnostic_cap) {
  (void)ctx;
  if (!radio_is_ready()) {
    snprintf(out_diagnostic, out_diagnostic_cap, "radio not ready (err=%d)", radio_last_error());
    return false;
  }
  snprintf(out_diagnostic, out_diagnostic_cap, "radio ready");
  return true;
}

/* boomlink_command_ops_t.clear_statistics - every counter this firmware
   actually keeps: the radio layer's own (radio_reset_stats()) and the link
   engine's (boomlink_link_reset_stats(), which - see its own doc - zeroes
   only counters, never the session/sequence/duplicate-cache state a live
   link depends on). boomlink_dispatch_t's stats have no reset of their own
   (boomlink_dispatch.h exposes no such call) and so are left alone.
   link_service_link() returning NULL (link engine not brought up) is not a
   failure of THIS command - the radio side still cleared - so this always
   reports success regardless. */
static bool cmd_clear_statistics(void *ctx) {
  (void)ctx;
  radio_reset_stats();
  boomlink_link_t *link = link_service_link();
  if (link != NULL) {
    boomlink_link_reset_stats(link);
  }
  return true;
}

/* boomlink_command_ops_t.request_diagnostics - a compact one-line summary
   of the state an operator would otherwise have to gather from `link
   status`/`radio status` separately. Bounded by out_diagnostic_cap the same
   as self_test() above; snprintf truncates safely rather than overflowing
   if a future field addition makes this run long. */
static bool cmd_request_diagnostics(void *ctx, char *out_diagnostic, size_t out_diagnostic_cap) {
  (void)ctx;
  boomlink_link_stats_t link_stats = {0};
  boomlink_link_t      *link       = link_service_link();
  if (link != NULL) {
    boomlink_link_get_stats(link, &link_stats);
  }
  radio_stats_t radio_stats;
  radio_get_stats(&radio_stats);
  snprintf(out_diagnostic, out_diagnostic_cap,
           "node=0x%08lX tx=%lu rx=%lu txfail=%lu rxcrc=%lu",
           (unsigned long)link_service_node_id(), (unsigned long)link_stats.tx_envelopes,
           (unsigned long)link_stats.rx_envelopes, (unsigned long)link_stats.tx_failures,
           (unsigned long)radio_stats.rx_crc_errors);
  return true;
}

void protocol_service_load_config(boomlink_node_config_t *out) {
  boomlink_flash_storage_port_init(&s_storage_port);
  if (!boomlink_config_store_load(&s_storage_port, out)) {
    boomlink_node_config_defaults(out);
  }
}

/* Persist `s_config_svc`'s PERSISTABLE snapshot as it stands right now -
   boomlink_config_service_get_persistable_config(), not _get_config():
   the two agree except while a hazardous change is WAITING for
   confirmation, when this call site can run for a completely UNRELATED
   accepted write (see protocol_service_on_rx()'s own comments on why
   persistence isn't gated on apply_state). Using plain `current` there
   would write that other change's still-unconfirmed node_id/magic/radio
   to flash - exactly the "survives a reboot with a value nobody
   confirmed" failure revert-on-timeout exists to prevent, found by
   review: an ordinary two-request provisioning sequence (stage a
   hazardous change, then an unrelated non-hazardous one before the first
   is confirmed) was enough to reach it, not merely a contrived one.
   Called from protocol_service_on_rx() at the two points a config change
   actually becomes this node's real, standing configuration - see those
   call sites' own comments for exactly which two and why not any of the
   others. Return value intentionally ignored: boomlink_config_store_
   save()'s own doc says a hardware failure here just means the region
   "look[s] invalid on the next load" (the same fallback path a fresh or
   corrupt save already takes), and this file has no diagnostic channel
   of its own to report a failure through outside of a CLI context - the
   same "not checked, no bring-up retry" posture link_service_init() and
   protocol_service_init() already take on their own failure paths. */
static void persist_current_config(void) {
  boomlink_node_config_t persistable;
  boomlink_config_service_get_persistable_config(&s_config_svc, &persistable);
  (void)boomlink_config_store_save(&s_storage_port, &persistable);
}

bool protocol_service_init(const boomlink_node_config_t *loaded_config) {
  if (loaded_config == NULL) {
    return false;
  }

  s_command_ops.reboot              = cmd_reboot;
  s_command_ops.identify            = NULL; /* no LED on this board to indicate with */
  s_command_ops.self_test           = cmd_self_test;
  s_command_ops.start_detection     = NULL; /* no detection algorithm exists yet */
  s_command_ops.stop_detection      = NULL; /* ditto */
  s_command_ops.clear_statistics    = cmd_clear_statistics;
  s_command_ops.request_diagnostics = cmd_request_diagnostics;
  /* every action reaches its state through file-static singletons, the
     same approach link_service.c takes - no per-instance ctx needed */
  s_command_ops.ctx = NULL;

  boomlink_config_service_init(&s_config_svc, loaded_config, PROTOCOL_SERVICE_CONFIRM_WINDOW_MS);

  boomlink_dispatch_handlers_t handlers = {0};
  handlers.on_command      = boomlink_command_service_handle;
  handlers.on_command_user = &s_command_ops;
  handlers.on_config       = boomlink_config_service_handle;
  handlers.on_config_user  = &s_config_svc;
  /* on_detection/on_telemetry/on_system left NULL: section 4's original PR 4
     scope for detection/telemetry was recognition-only (no algorithm, no
     sensor backing exists - see boomlink.md's Phase C notes), and nothing
     in this protocol yet needs an automatic System/Ping responder. */
  boomlink_dispatch_init(&s_dispatch, &handlers);

  s_reboot_armed     = false;
  s_confirm_eligible = false;
  s_initialized      = true;
  return true;
}

void protocol_service_on_rx(void *user, uint32_t source_id, uint32_t destination_id,
                            const uint8_t *payload, size_t payload_len, float rssi_dbm,
                            float snr_db) {
  (void)user;
  if (!s_initialized) {
    return;
  }

  boomlink_Envelope envelope = boomlink_Envelope_init_zero;
  if (!boomlink_decode_envelope(payload, payload_len, &envelope)) {
    return;
  }

  boomlink_dispatch_rx_info_t rx = {
    .source_id      = source_id,
    .destination_id = destination_id,
    .rssi_dbm       = rssi_dbm,
    .snr_db         = snr_db,
  };

  boomlink_dispatch_result_t result = boomlink_dispatch_process(&s_dispatch, &rx, &envelope);
  if (!result.has_response) {
    return;
  }

  /* A ConfigSet's non-hazardous fields already applied to `current`
     unconditionally, inside boomlink_dispatch_process() above - section
     8.2's own "applies non-hazardous fields right away" rule, independent
     of whether the response below manages to reach the requester, and
     independent of whether the SAME request ALSO carried a hazardous field
     (OK and PENDING_CONFIRMATION are the two results a real, accepted
     write can produce - see boomlink_config_service_handle()'s own doc;
     everything else is a rejection that touched nothing). Persist now, not
     gated on the send-success check further down: that gating exists for
     the hazardous field ITSELF, where section 8.2 requires waiting for
     send success BEFORE it takes effect at all - a non-hazardous field has
     no such rule, it is already live, and flash must reflect that
     regardless of what happens to this particular reply.

     Persisting here on PENDING_CONFIRMATION too (not only OK) is what
     keeps a bundled request honest: a single ConfigSet naming BOTH a
     non-hazardous field (say, telemetry.report_interval_s) and a
     hazardous one (say, GeneralConfig.node_id) answers PENDING_
     CONFIRMATION for the whole request, even though the non-hazardous
     field already applied. If persistence only fired on OK, and the
     hazardous half later timed out unconfirmed, boomlink_config_service_
     poll()'s revert only restores the hazard subset (node_id/magic/radio)
     - never persisted at all, the non-hazardous field would still be
     silently lost on the NEXT reboot, with `current` and flash agreeing
     for the rest of THIS session and nothing ever telling the operator
     otherwise.

     persist_current_config() calls boomlink_config_service_get_
     persistable_config(), not the plain current-state accessor, which is
     what makes this call site safe not only for THIS request (whose own
     hazard subset, if any, is still its OLD pre-commit value in `current`
     at this exact point - commit_pending_apply() hasn't run yet, see
     below) but also for an entirely UNRELATED accepted write arriving
     while a DIFFERENT hazardous change from an EARLIER request is still
     WAITING for confirmation: without that accessor, persisting `current`
     directly in that case would write the other change's still-
     unconfirmed hazard value to flash, surviving a reboot even after this
     node's own later revert-on-timeout restores it in RAM only - found by
     review as a real, reachable gap (an ordinary two-request provisioning
     sequence, not a contrived one), not a hypothetical worth guessing
     past. */
  bool is_accepted_config_write =
      result.response.which_payload == boomlink_Envelope_config_tag &&
      result.response.payload.config.which_message == boomlink_ConfigMessage_set_response_tag &&
      (result.response.payload.config.message.set_response.result ==
           boomlink_ConfigSetResult_CONFIG_SET_RESULT_OK ||
       result.response.payload.config.message.set_response.result ==
           boomlink_ConfigSetResult_CONFIG_SET_RESULT_PENDING_CONFIRMATION);
  if (is_accepted_config_write) {
    persist_current_config();
  }

  uint8_t buf[boomlink_Envelope_size];
  size_t  len = 0;
  if (!boomlink_encode_envelope(&result.response, buf, sizeof(buf), &len)) {
    return;
  }

  boomlink_link_t *link = link_service_link();
  if (link == NULL) {
    return;
  }

  /* boomlink_txqueue.h's documented priority mapping: HIGH for a command
     response, NORMAL for everything else this file ever sends (today, only
     a config response). */
  bool is_command_response  = result.response.which_payload == boomlink_Envelope_command_tag;
  boomlink_tx_priority_t priority =
      is_command_response ? BOOMLINK_TXPRIO_HIGH : BOOMLINK_TXPRIO_NORMAL;

  /* request_ack is true for an ordinary unicast requester (`source_id` is
     always a specific address, never BOOMLINK_ADDR_BROADCAST, since that is
     who this replies TO, not a destination this file chose): section 8.3's
     "commands... return a correlated response" and section 8.2's
     revert-on-timeout both depend on the requester actually receiving this,
     which an unacknowledged best-effort send cannot promise.
     request_ack is forced OFF when `destination_id` - the INCOMING
     request's own addressing, not this response's - was broadcast: every
     "dangerous over broadcast" rejection in boomlink_command_service.c/
     boomlink_config_service.c justifies itself by "simultaneous
     responses... need a separate design", but boomlink_dispatch_process()
     builds and sends a response for a rejected request exactly like any
     other (a rejection IS a response, section 8.3's own "a response is
     always built" rule) - so without this, every reachable node still
     answers a broadcast frame with an ACK-requested, retried-up-to-
     LINK_MAX_ATTEMPTS-times response, which is precisely the "N
     simultaneous responses" those rejections claim to prevent, just moved
     from N accepted actions to N retried refusals (and, for a broadcast
     ConfigGet - never rejected, since it mutates nothing - N legitimate
     answers). Forcing this off is what actually closes that: a broadcast
     requester gets one best-effort reply per reachable node, not up to
     three each. */
  bool request_ack = destination_id != BOOMLINK_ADDR_BROADCAST;

  boomlink_link_send_result_t send_rc =
      boomlink_link_send(link, source_id, priority, request_ack, buf, len);
  bool queued = (send_rc == BOOMLINK_LINK_SEND_OK || send_rc == BOOMLINK_LINK_SEND_OK_EVICTED);
  if (!queued) {
    return;
  }

  /* section 8.2's revert-on-timeout apply, and what "the response confirming
     the change has been transmitted" (boomlink_config_service_commit_
     pending_apply()'s own doc) and "confirmed... actually works"
     (boomlink_config_service_confirm_pending_apply()'s own doc) mean in
     THIS phase - a decision that module explicitly leaves to its caller
     rather than making itself.

     Commit as soon as boomlink_link_send() reports the response QUEUED, not
     once it is actually on the air: boomlink_link_tx_done_fn's own doc
     rules out correlating a specific queued frame with a later on_tx_done
     event by anything available before it fires (`sequence` "is NOT known
     until this callback fires"), and cli.c's tx_done callback is shared
     with unrelated traffic (`link ping`) this file has no way to
     distinguish from its own response. The alternative - never committing
     without that correlation - is worse: boomlink_config_service_poll()'s
     STAGED-abandon path means an uncommitted stage times out and reverts on
     its own anyway, silently defeating every hazardous config write. A
     response that is queued at HIGH/NORMAL priority (at or above everything
     else this bring-up firmware ever sends) and then never actually
     transmits is already an unlikely failure this codebase has no better
     tool to detect than that same abandon path.

     Confirm on the next request/response exchange this file completes
     (queues a response for) with ANY peer while WAITING - deliberately
     loose, and honestly incomplete: no live radio/node_id/magic
     reconfiguration exists yet (App/radio/radio.h has no setter, and
     boomlink_link_reconfigure() explicitly excludes node_id/magic - see its
     own doc), so a hazardous change here only ever takes effect on the NEXT
     boot's protocol_service_load_config(), not in this running session.
     This confirmation therefore proves the CURRENT (pre-change) session
     remains reachable, not that the new profile will still work after that
     next reboot - a real closed-loop check would need the pending state to
     survive the reboot itself and be confirmed afterwards, which is out of
     scope for this phase and not attempted here.

     `s_confirm_eligible` closes a narrower gap in "the next exchange": with
     it, this would be reachable within the very SAME boomlink_link_poll()
     drain that just committed - that function drains every packet the
     radio's ring buffer holds in one call (boomlink_link.h's own doc), so
     two frames already sitting in the ring (a hazardous ConfigSet, then
     literally anything else that gets a response queued - even an
     unrelated request, or a second hazardous ConfigSet this service itself
     rejects as APPLY_IN_PROGRESS) would otherwise let the whole IDLE ->
     STAGED -> WAITING -> IDLE cycle complete before a single bit of the
     PENDING_CONFIRMATION response has actually reached the air, proving
     nothing about reachability at all. Cleared false at the moment of
     commit and only ever set true by protocol_service_process() - which
     cli_process() calls once per tick, strictly AFTER the same tick's
     link_service_process() (and therefore boomlink_link_poll()) has
     already finished draining - so the earliest a confirm can fire is a
     frame from the NEXT tick's drain, never one still sitting in the
     current burst. */
  bool is_pending_config = result.response.which_payload == boomlink_Envelope_config_tag &&
                           result.response.payload.config.which_message ==
                               boomlink_ConfigMessage_set_response_tag &&
                           result.response.payload.config.message.set_response.result ==
                               boomlink_ConfigSetResult_CONFIG_SET_RESULT_PENDING_CONFIRMATION;
  if (is_pending_config) {
    boomlink_config_service_commit_pending_apply(&s_config_svc, HAL_GetTick());
    s_confirm_eligible = false;
    /* No persist call here, even though the block above already persisted
       for this same response (is_accepted_config_write covers PENDING_
       CONFIRMATION too): commit_pending_apply() just moved apply_state to
       WAITING, so a persist here would report the exact same bytes the
       earlier call already wrote (get_persistable_config() now falls back
       to `revert_to` for the hazard subset in WAITING, the same values
       `current` held at that earlier call) - redundant, not merely safe.
       The snapshot already on flash (old hazard fields + this response's
       newly-applied non-hazard fields) stays correct until a real confirm
       below persists the hazard subset's new value too - or, if this
       times out instead, stays correct forever, since nothing about it
       was ever wrong. */
  } else if (!s_reboot_armed && s_confirm_eligible &&
             boomlink_config_service_apply_state(&s_config_svc) == BOOMLINK_CONFIG_APPLY_WAITING) {
    /* Order matters: confirm_pending_apply() moves apply_state to IDLE
       BEFORE this persists. get_persistable_config() only overrides the
       hazard subset while WAITING, so persisting one statement earlier
       (still WAITING) would write the OLD hazard values right after
       telling the requester the change is confirmed - persist must see
       IDLE to write the real, newly-confirmed ones.

       !s_reboot_armed matters for a reason unrelated to ordering: cmd_
       reboot() already ran (inside boomlink_dispatch_process() above) by
       the time this else-if is reached for a Reboot request's own
       response, so without this guard, the very "next exchange" that
       confirms a pending hazardous change could be the Reboot command
       that then resets the node PROTOCOL_SERVICE_REBOOT_DELAY_MS later -
       confirming AND persisting the new value, then rebooting into it
       before the PROTOCOL_SERVICE_CONFIRM_WINDOW_MS revert-on-timeout
       this whole mechanism exists to provide could ever engage. Found by
       review: whether an in-flight hazardous change survives a Reboot
       arriving inside its confirm window would otherwise depend on pure
       RX-drain timing (this same tick's drain vs. a later one) with
       nothing operator-visible telling the two cases apart, reaching
       exactly the "stranding a remote node on a profile nobody else uses"
       failure section 8.2's own revert-on-timeout design names as the
       reason it exists - by the one route (a real hardware reset) that
       revert-on-timeout categorically cannot outrun once armed. Skipping
       the confirm here is safe, not merely less bad: s_reboot_armed is
       only ever set, never cleared, so this node is going down in
       PROTOCOL_SERVICE_REBOOT_DELAY_MS regardless of this branch, and
       leaving the change un-confirmed simply means it reverts to
       `revert_to` on this same reboot - the value already on flash,
       since persist_current_config() above never wrote the unconfirmed
       one - the same outcome a timeout would have produced anyway, made
       deterministic instead of a timing coin flip. */
    boomlink_config_service_confirm_pending_apply(&s_config_svc);
    persist_current_config();
  }
}

void protocol_service_process(void) {
  if (!s_initialized) {
    return;
  }

  uint32_t now_ms = HAL_GetTick();
  boomlink_config_service_poll(&s_config_svc, now_ms);

  /* See protocol_service_on_rx()'s long comment on `s_confirm_eligible`:
     this runs once per superloop tick, always after that same tick's RX
     drain has already finished, so setting this unconditionally true here
     only ever makes a confirm eligible starting with the NEXT tick's
     drain - never the one a commit just happened in. Harmless to set when
     no change is WAITING at all; the read side only consults it then. */
  s_confirm_eligible = true;

  /* Unsigned subtraction is wrap-safe for any real elapsed span - the same
     idiom boomlink_config_service.c's own window_elapsed() uses. */
  if (s_reboot_armed &&
      (uint32_t)(now_ms - s_reboot_armed_at_ms) >= PROTOCOL_SERVICE_REBOOT_DELAY_MS) {
    HAL_NVIC_SystemReset();
  }
}

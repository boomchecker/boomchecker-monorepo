/**
 ******************************************************************************
 * @file    boomlink_config_service.c
 ******************************************************************************
 */
#include "boomlink_config_service.h"

#include <math.h>

/* For BOOMLINK_ADDR_INVALID/BOOMLINK_ADDR_BROADCAST (boomlink.md section
   7.2) - reused rather than duplicated as bare hex literals, the same
   "read the real value, don't hardcode a second copy" reasoning
   boomlink_codec.h gives for BOOMLINK_RADIO_MAX_PAYLOAD. Header-only use:
   this pulls in no Nanopb dependency of its own (linkframe has none, by
   section 9's rule), so it does not affect this module's own Nanopb
   dependency in either direction. */
#include "boomlink_linkframe.h"

void boomlink_node_config_defaults(boomlink_node_config_t *out) {
  if (out == NULL) {
    return;
  }
  *out                 = (boomlink_node_config_t){0};
  out->config_version  = 1u; /* 0 is the "nobody set this" accident value, not a real version */
  /* Every group is always present in a real NodeConfig - has_X only means
     "was this present" because Nanopb generates it for every message-type
     field regardless of whether this type has any partial-presence use for
     it (see the type's own doc). Left false here, boomlink_config_store_
     save() would Nanopb-encode an empty message for every group instead of
     its all-zero-but-real value - decodable, but not what a reader of the
     stored blob would expect "the default GeneralConfig" to look like. */
  out->has_general     = true;
  out->has_link         = true;
  out->has_radio        = true;
  out->has_detection    = true;
  out->has_gnss         = true;
  out->has_telemetry    = true;
  /* Same reasoning one level deeper: DetectionConfig's own two submessages
     (DroneDetectionConfig/GunshotDetectionConfig) are themselves singular
     message-type fields, so Nanopb gives THEM a has_X too - and this
     function already forces has_detection true above for exactly the
     reason this comment gives for the outer six groups. Left false here,
     a save() would still encode a real (if empty) DetectionConfig, but
     silently drop both of ITS submessages instead of their all-zero-but-
     real values - the identical gap this function exists to close, found
     one nesting level down. */
  out->detection.has_drone   = true;
  out->detection.has_gunshot = true;
  /* Same "defaults() must agree with the real hardcoded/running value"
     reasoning as magic/link/radio below, for GeneralConfig's two link-
     enable flags - App/link/link_service.c's s_enabled defaults true and
     gates BOTH RX and TX together (link_service_process() skips
     boomlink_link_poll() entirely while disabled); nothing gates
     transmit_enabled independently at all today, so this node transmits
     regardless of this field's value - true is what actually happens
     either way. Left false before this fix: a fresh node's first
     ConfigGet reported a node that can neither receive nor transmit,
     despite demonstrably doing both to answer that very GET - and
     boomlink.md's own canonical sensor-node and gateway example configs
     already show both fields true, not the struct's zero-init. This is
     PR 4 Phase C's own first-ever caller of boomlink_config_store_save()
     - the last moment this is free to fix, before a wrong default is
     ever actually written to a real node's flash. */
  out->general.receive_enabled  = true;
  out->general.transmit_enabled = true;
  /* Not left at the zero-init `{0}` gave it: 0 is not a real magic value,
     it is "this field was never set" - the same distinction node_id draws
     against BOOMLINK_ADDR_INVALID (also 0) two lines below in spirit, if
     not in this function's own code. App/link/link_service.c's actual
     bring-up default (before PR 4 Phase C wired NodeConfig into it at all)
     was already BOOMLINK_LINKFRAME_MAGIC_DEFAULT, hardcoded there - a fresh
     node with no persisted config, or one that fails to load, needs
     defaults() to agree with what bring-up already did, or Phase C wiring
     this in would silently change a fresh node's magic from 0xB0 to 0. */
  out->link.magic      = BOOMLINK_LINKFRAME_MAGIC_DEFAULT;
  /* The same "defaults() must agree with the real hardcoded bring-up value"
     reasoning as magic above, for LinkConfig's five other fields - config.
     proto's own doc says this message "mirrors boomlink_link_config_t's
     five runtime-reconfigurable fields", and App/link/link_service.c's
     LINK_* macros are that mirror's real values. Left at 0 before this fix:
     a ConfigGet on a fresh/defaulted node reported ack_timeout_margin_ms=0/
     max_attempts=0/backoff_min_ms=0/backoff_max_ms=0/tx_jitter_max_ms=0,
     none of which match what the link engine actually runs with - the
     exact "reported config lies about the real bring-up value" gap this
     function's magic fix exists to close, just for five more fields.
     (Nothing yet applies a LATER change to these fields to the live link
     engine either - boomlink_link_reconfigure() is never called from this
     firmware - but that is a separate, documented gap; this fix is only
     about the DEFAULT value matching reality.) */
  out->link.ack_timeout_margin_ms = 50u;
  out->link.max_attempts          = 3u;
  out->link.backoff_min_ms        = 100u;
  out->link.backoff_max_ms        = 400u;
  out->link.tx_jitter_max_ms      = 50u;
  /* Same reasoning again for RadioConfig - config.proto's own doc says it
     "mirrors radio_profile_t", and App/radio/e22_radio.cpp's
     DefaultProfile() is that mirror's real value (the profile radio_init()
     actually programs into the SX1262 at boot). Pre-existing gap (not
     introduced by this fix), but now reachable over the air for the first
     time as of PR 4 Phase C wiring ConfigGet/ConfigSet to a real link, so
     it gets the same treatment here rather than being left for a later
     PR to rediscover. */
  out->radio.frequency_mhz     = 869.525f;
  out->radio.bandwidth_khz     = 125.0f;
  out->radio.spreading_factor  = 7u;
  out->radio.coding_rate_denom = 5u;
  out->radio.tx_power_dbm      = 14;
  out->radio.preamble_symbols  = 8u;
  out->radio.sync_word         = 0x12u; /* RADIOLIB_SX126X_SYNC_WORD_PRIVATE */
}

void boomlink_config_service_init(boomlink_config_service_t *svc,
                                  const boomlink_node_config_t *initial,
                                  uint32_t confirm_window_ms) {
  if (svc == NULL) {
    return;
  }
  svc->current = (initial != NULL) ? *initial : (boomlink_node_config_t){0};
  if (initial == NULL) {
    boomlink_node_config_defaults(&svc->current);
  }
  svc->apply_state         = BOOMLINK_CONFIG_APPLY_IDLE;
  svc->staged              = (boomlink_config_hazard_t){0};
  svc->revert_to           = (boomlink_config_hazard_t){0};
  svc->apply_started_at_ms = 0;
  svc->staged_seen         = false;
  svc->confirm_window_ms   = confirm_window_ms;
}

static boomlink_config_hazard_t hazard_snapshot(const boomlink_node_config_t *cfg) {
  boomlink_config_hazard_t h;
  h.node_id = cfg->general.node_id;
  h.magic   = cfg->link.magic;
  h.radio   = cfg->radio;
  return h;
}

/* `==`, except two NaNs also compare equal - not full IEEE-754 semantics,
   RESTORING them where this specific use needs it. IEEE-754 `==` is not
   reflexive (NaN == NaN is false), but this comparison is asked "is this
   still the same value as itself" every time a SET leaves a field
   untouched (requested_hazard starts as a byte copy of current_hazard - see
   handle_set() below), and RadioConfig's numeric ranges are deliberately
   not validated yet (config.proto's own CONFIG_SET_RESULT_INVALID comment),
   so a NaN CAN legitimately end up in current.radio. Plain `==` there would
   never be reflexive again once that happened: every later SET, however
   unrelated, would misreport hazard_changed=true forever, with no timeout
   recovery (poll() does not revert a merely-STAGED, uncommitted change) -
   confirmed by staging and confirming a NaN frequency_mhz, then observing a
   completely unrelated telemetry-only SET come back PENDING_CONFIRMATION
   instead of OK. */
static bool float_equal_or_both_nan(float a, float b) {
  return a == b || (isnan(a) && isnan(b));
}

/* Field-by-field, not memcmp() on the whole struct: IEEE-754 equality and
   bit-pattern equality disagree on signed zero (-0.0f == 0.0f is true;
   their bit patterns are not), so memcmp() can misreport a
   numerically-unchanged resend as a hazardous change. Confirmed: a
   RadioConfig differing only by +0.0f vs -0.0f in frequency_mhz staged a
   PENDING_CONFIRMATION and blocked every other config write until it was
   committed or timed out, even though nothing about the profile actually
   changed. */
static bool radio_config_equal(const boomlink_RadioConfig *a, const boomlink_RadioConfig *b) {
  return float_equal_or_both_nan(a->frequency_mhz, b->frequency_mhz) &&
         float_equal_or_both_nan(a->bandwidth_khz, b->bandwidth_khz) &&
         a->spreading_factor == b->spreading_factor &&
         a->coding_rate_denom == b->coding_rate_denom && a->tx_power_dbm == b->tx_power_dbm &&
         a->preamble_symbols == b->preamble_symbols && a->sync_word == b->sync_word;
}

static bool hazard_equal(const boomlink_config_hazard_t *a, const boomlink_config_hazard_t *b) {
  return a->node_id == b->node_id && a->magic == b->magic && radio_config_equal(&a->radio, &b->radio);
}

static bool handle_get(const boomlink_config_service_t *svc, const boomlink_ConfigGetRequest *req,
                       boomlink_ConfigMessage *out_response) {
  out_response->which_message           = boomlink_ConfigMessage_get_response_tag;
  boomlink_ConfigGetResponse *resp      = &out_response->message.get_response;
  *resp                                 = (boomlink_ConfigGetResponse){0};
  resp->config_version                  = svc->current.config_version;

  if (req->include_general) {
    resp->has_general = true;
    resp->general      = svc->current.general;
  }
  if (req->include_link) {
    resp->has_link = true;
    resp->link      = svc->current.link;
  }
  if (req->include_radio) {
    resp->has_radio = true;
    resp->radio      = svc->current.radio;
  }
  if (req->include_detection) {
    resp->has_detection = true;
    resp->detection      = svc->current.detection;
  }
  if (req->include_gnss) {
    resp->has_gnss = true;
    resp->gnss      = svc->current.gnss;
  }
  if (req->include_telemetry) {
    resp->has_telemetry = true;
    resp->telemetry      = svc->current.telemetry;
  }
  return true;
}

static void respond_set(boomlink_ConfigMessage *out_response, boomlink_ConfigSetResult result,
                        uint32_t config_version) {
  out_response->which_message      = boomlink_ConfigMessage_set_response_tag;
  boomlink_ConfigSetResponse *resp = &out_response->message.set_response;
  resp->result                     = result;
  resp->config_version             = config_version;
}

/* node_id must be outside the two reserved addresses (boomlink.md section
   7.2: 0x00000000 is unconfigured, 0xFFFFFFFF is broadcast) to ever be a
   node's OWN identity - the same rule boomlink_linkframe.c's
   is_valid_node_id() enforces for the wire header this becomes at Phase C.
   magic is one wire byte (section 7.3); config.proto's own comment already
   flags the uint32-for-proto-ergonomics mismatch this checks. */
static bool node_id_is_valid(uint32_t node_id) {
  return node_id != BOOMLINK_ADDR_INVALID && node_id != BOOMLINK_ADDR_BROADCAST;
}

static bool magic_is_valid(uint32_t magic) {
  /* 0 is rejected for the same reason node_id_is_valid() rejects
     BOOMLINK_ADDR_INVALID (also 0): App/link/link_service.c's
     link_service_init() treats a configured magic of 0 as "never set,
     fall back to BOOMLINK_LINKFRAME_MAGIC_DEFAULT" (see that function's
     own doc). Before this check, a ConfigSet could write magic=0 into
     `current` - reported back by every later ConfigGet as the node's real,
     committed magic - while the next reboot would silently run with
     0xB0 instead, permanently diverging what is persisted/reported from
     what is actually running. */
  return magic != 0u && magic <= 0xFFu;
}

static bool handle_set(boomlink_config_service_t *svc, const boomlink_dispatch_rx_info_t *rx,
                       const boomlink_ConfigSetRequest *req,
                       boomlink_ConfigMessage *out_response) {
  /* boomlink.md's own PR 4 notes: "a broadcast ConfigSet must not be added
     casually because simultaneous responses and coordinated radio-profile
     changes require a separate design [this codebase] does not have yet."
     Rejected outright, before anything else in this function - not scoped
     to hazardous fields only: a non-hazardous broadcast SET still provokes
     every reachable node to answer at once on one shared channel, and (as
     of this PR's config-persistence wiring) to independently erase+rewrite
     its own flash sector, all from a single unauthenticated frame nothing
     in this protocol yet authenticates. The same pattern boomlink_command_
     service.c's command_is_dangerous_over_broadcast() uses, applied here
     to the one remaining unguarded write path that table doesn't cover.
     ConfigGetRequest is deliberately NOT rejected this way - handle_get()
     mutates nothing, so this concern does not apply to it. */
  if (rx != NULL && rx->destination_id == BOOMLINK_ADDR_BROADCAST) {
    respond_set(out_response, boomlink_ConfigSetResult_CONFIG_SET_RESULT_INVALID,
               svc->current.config_version);
    return true;
  }

  if (req->expected_config_version != svc->current.config_version) {
    respond_set(out_response, boomlink_ConfigSetResult_CONFIG_SET_RESULT_VERSION_CONFLICT,
               svc->current.config_version);
    return true;
  }

  /* Computed before touching anything else: both the APPLY_IN_PROGRESS gate
     and the validation below need to know whether THIS request actually
     asks for a hazardous change, not just whether it mentions a hazardous
     group - see config.proto's own "whole-group replacement" doc, which is
     exactly why has_general/has_link/has_radio alone cannot answer that. */
  boomlink_config_hazard_t current_hazard   = hazard_snapshot(&svc->current);
  boomlink_config_hazard_t requested_hazard = current_hazard;
  if (req->has_general) {
    requested_hazard.node_id = req->general.node_id;
  }
  if (req->has_link) {
    requested_hazard.magic = req->link.magic;
  }
  if (req->has_radio) {
    requested_hazard.radio = req->radio;
  }
  bool hazard_changed = !hazard_equal(&requested_hazard, &current_hazard);

  /* Only a request that would ITSELF introduce or change a hazardous value
     conflicts with one already pending - section 8.2 names no policy for
     overlapping HAZARDOUS changes and guessing one risks a revert that
     restores neither state, but nothing in that reasoning extends to an
     unrelated non-hazardous write arriving in the same window (e.g. a
     gunshot-threshold tweak while a radio-profile change is still waiting
     on its confirmation). Rejecting those too would make one hazardous
     change lock out all remote configuration for the whole confirm window. */
  if (hazard_changed && svc->apply_state != BOOMLINK_CONFIG_APPLY_IDLE) {
    respond_set(out_response, boomlink_ConfigSetResult_CONFIG_SET_RESULT_APPLY_IN_PROGRESS,
               svc->current.config_version);
    return true;
  }

  /* Validate only the delta being requested, not a resend of an unchanged
     value: a caller editing an unrelated field of GeneralConfig/LinkConfig
     must resend the whole group per the whole-group-replacement contract,
     and a node that has never been assigned an ID legitimately still has
     node_id == BOOMLINK_ADDR_INVALID to resend untouched. Rejecting that
     resend would make it impossible to configure anything else about a
     freshly-provisioned node before it has an identity. */
  bool node_id_changing = req->has_general && requested_hazard.node_id != current_hazard.node_id;
  bool magic_changing   = req->has_link && requested_hazard.magic != current_hazard.magic;
  if ((node_id_changing && !node_id_is_valid(requested_hazard.node_id)) ||
      (magic_changing && !magic_is_valid(requested_hazard.magic))) {
    respond_set(out_response, boomlink_ConfigSetResult_CONFIG_SET_RESULT_INVALID,
               svc->current.config_version);
    return true;
  }

  /* Apply every NON-hazardous field immediately - only node_id/magic/radio
     wait for confirmation. A submessage's presence (has_X) is "this group
     was included in the write", independent of whether any hazardous field
     inside it actually changed value; the two are checked separately above.

     Whole-GROUP replacement, not a per-field patch: `req->general` present
     means "this is GeneralConfig's new value, in full" - proto3 gives no
     way to tell "the caller left node_id at its zero default" apart from
     "the caller means node_id = 0" for a plain scalar, so a per-field patch
     built on presence would silently zero every field the caller did not
     bother restating. A caller changing one field is expected to GET the
     current group first and send the whole thing back with that one field
     edited - see config.proto's ConfigSetRequest doc. */
  /* next.has_X set alongside each next.X assignment below, not left to
     whatever svc->current.has_X already was: `next.X = req->X` only copies
     the submessage's VALUE, and boomlink_node_config_t's has_X only ever
     starts true because boomlink_node_config_defaults() forces it there -
     a future caller of boomlink_config_service_init() that ever seeded
     svc->current from a hand-built config with some has_X false (nothing
     today does; boomlink_config_service.h's own doc already says this is
     the caller's responsibility) would otherwise never see it self-heal on
     a later SET. That matters for Phase B specifically: an
     always-true-and-forgotten-about has_X is invisible to every reader in
     THIS file (nothing here ever branches on next.has_X), but Nanopb skips
     encoding a singular message field whose has_X is false, so
     boomlink_config_store_save() would silently drop that whole group from
     the persisted blob - it would read back as all-zero defaults on the
     next boot even though every GET in the current session answered
     correctly. */
  boomlink_node_config_t next = svc->current;
  if (req->has_general) {
    next.general         = req->general;
    next.general.node_id = svc->current.general.node_id; /* hazardous - restored below if changed */
    next.has_general     = true;
  }
  if (req->has_link) {
    next.link       = req->link;
    next.link.magic = svc->current.link.magic; /* hazardous - restored below if changed */
    next.has_link   = true;
  }
  if (req->has_detection) {
    next.detection     = req->detection;
    next.has_detection = true;
  }
  if (req->has_gnss) {
    next.gnss     = req->gnss;
    next.has_gnss = true;
  }
  if (req->has_telemetry) {
    next.telemetry     = req->telemetry;
    next.has_telemetry = true;
  }
  /* RadioConfig is entirely hazardous - `next.radio`'s VALUE is deliberately
     NOT touched here, only in the staged snapshot (or applied directly by
     commit_pending_apply()/poll()'s revert, both of which assert has_radio
     themselves - see their own comments). But has_radio still needs
     asserting here too, on its own, for the one case neither of those
     reaches: a SET that includes RadioConfig with a value EQUAL to
     current's (not hazardous - hazard_changed only compares values, not
     presence) never stages anything at all, so if nothing else in the same
     request is hazardous either, this function returns OK having never
     touched svc->current.radio - correctly, since the value didn't change -
     but that is still "this group was included in the write" by the same
     whole-group-replacement contract has_general/has_link/etc. use above,
     and has_radio must reflect that regardless of whether the value moved. */
  if (req->has_radio) {
    next.has_radio = true;
  }

  next.config_version = svc->current.config_version + 1u;
  svc->current         = next;

  if (!hazard_changed) {
    respond_set(out_response, boomlink_ConfigSetResult_CONFIG_SET_RESULT_OK,
               svc->current.config_version);
    return true;
  }

  /* hazard_changed here implies apply_state was IDLE (the only other case
     already returned APPLY_IN_PROGRESS above), so this can never clobber an
     already-pending stage. */
  svc->apply_state = BOOMLINK_CONFIG_APPLY_STAGED;
  svc->staged       = requested_hazard;
  svc->revert_to    = current_hazard;
  svc->staged_seen  = false; /* poll() has not observed this stage yet */
  respond_set(out_response, boomlink_ConfigSetResult_CONFIG_SET_RESULT_PENDING_CONFIRMATION,
             svc->current.config_version);
  return true;
}

bool boomlink_config_service_handle(void *user, const boomlink_dispatch_rx_info_t *rx,
                                    const boomlink_ConfigMessage *request,
                                    boomlink_ConfigMessage *out_response) {
  boomlink_config_service_t *svc = (boomlink_config_service_t *)user;
  if (svc == NULL || out_response == NULL || request == NULL) {
    return false;
  }

  switch (request->which_message) {
    case boomlink_ConfigMessage_get_request_tag:
      return handle_get(svc, &request->message.get_request, out_response);
    case boomlink_ConfigMessage_set_request_tag:
      return handle_set(svc, rx, &request->message.set_request, out_response);
    default:
      return false;
  }
}

void boomlink_config_service_commit_pending_apply(boomlink_config_service_t *svc,
                                                  uint32_t now_ms) {
  if (svc == NULL || svc->apply_state != BOOMLINK_CONFIG_APPLY_STAGED) {
    return;
  }
  svc->current.general.node_id  = svc->staged.node_id;
  svc->current.link.magic       = svc->staged.magic;
  svc->current.radio            = svc->staged.radio;
  /* has_general/has_link are already re-asserted by handle_set()'s
     immediate-assignment block (a hazardous node_id/magic change requires
     has_general/has_link true to even be detected as one) - has_radio has
     no such immediate-assignment path to ride along with, since RadioConfig
     is entirely deferred to this function, so it is asserted here on its
     own. Same reasoning as handle_set()'s own has_X reassignment: harmless
     today (every real svc->current starts from boomlink_node_config_
     defaults(), which already forces this true), but this is where
     RadioConfig's actual value lands, so this is where its presence flag
     must be guaranteed too - see this file's history for the five other
     groups this exact class of gap was fixed for. */
  svc->current.has_radio        = true;
  svc->apply_started_at_ms      = now_ms; /* overwrites any STAGED-phase latch from poll() */
  svc->apply_state              = BOOMLINK_CONFIG_APPLY_WAITING;
}

void boomlink_config_service_confirm_pending_apply(boomlink_config_service_t *svc) {
  if (svc == NULL || svc->apply_state != BOOMLINK_CONFIG_APPLY_WAITING) {
    return;
  }
  svc->apply_state = BOOMLINK_CONFIG_APPLY_IDLE;
}

/* Unsigned subtraction is wrap-safe for any real elapsed span - see
   fw/common/boomlink/linkengine/boomlink_port.h's boomlink_elapsed_ms() for
   the full reasoning (not linked here: that would pull a services module
   into depending on the link engine for one line of arithmetic that is
   exactly as correct inlined). */
static bool window_elapsed(uint32_t now_ms, uint32_t started_at_ms, uint32_t window_ms) {
  return (uint32_t)(now_ms - started_at_ms) >= window_ms;
}

void boomlink_config_service_poll(boomlink_config_service_t *svc, uint32_t now_ms) {
  if (svc == NULL || svc->apply_state == BOOMLINK_CONFIG_APPLY_IDLE) {
    return;
  }

  if (svc->apply_state == BOOMLINK_CONFIG_APPLY_STAGED) {
    /* See this function's own header doc for why this exists at all: a
       stage nobody ever commits must not block configuration forever. */
    if (!svc->staged_seen) {
      svc->apply_started_at_ms = now_ms;
      svc->staged_seen         = true;
      /* Falls through rather than returning: elapsed is exactly 0 right
         after this latch, so the only way that can already satisfy
         window_elapsed() is confirm_window_ms == 0 - and a zero-length
         window means "abandon immediately", not "abandon on the NEXT
         poll", the same immediacy WAITING's own timeout already gets on
         its first poll after commit_pending_apply(). Any real (non-zero)
         window still falls through to the same "not yet" return below. */
    }
    if (!window_elapsed(now_ms, svc->apply_started_at_ms, svc->confirm_window_ms)) {
      return;
    }
    /* Staging never touched `current` - only the bookkeeping needs dropping,
       there is no hazardous value in `current` to revert. */
    svc->apply_state = BOOMLINK_CONFIG_APPLY_IDLE;
    svc->staged_seen = false;
    return;
  }

  /* WAITING */
  if (!window_elapsed(now_ms, svc->apply_started_at_ms, svc->confirm_window_ms)) {
    return;
  }
  svc->current.general.node_id = svc->revert_to.node_id;
  svc->current.link.magic      = svc->revert_to.magic;
  svc->current.radio           = svc->revert_to.radio;
  svc->current.has_radio       = true; /* same reasoning as commit_pending_apply()'s own assertion */
  /* The revert is itself an observable state change - a client holding the
     config_version from the earlier PENDING_CONFIRMATION response must not
     be able to treat it as still current once the hazardous fields it
     described have reverted underneath it. */
  svc->current.config_version += 1u;
  svc->apply_state             = BOOMLINK_CONFIG_APPLY_IDLE;
}

void boomlink_config_service_get_config(const boomlink_config_service_t *svc,
                                        boomlink_node_config_t *out) {
  if (out == NULL) {
    return;
  }
  *out = (svc != NULL) ? svc->current : (boomlink_node_config_t){0};
}

boomlink_config_apply_state_t boomlink_config_service_apply_state(
    const boomlink_config_service_t *svc) {
  return (svc != NULL) ? svc->apply_state : BOOMLINK_CONFIG_APPLY_IDLE;
}

void boomlink_config_service_get_persistable_config(const boomlink_config_service_t *svc,
                                                     boomlink_node_config_t *out) {
  if (out == NULL) {
    return;
  }
  if (svc == NULL) {
    *out = (boomlink_node_config_t){0};
    return;
  }
  *out = svc->current;
  if (svc->apply_state == BOOMLINK_CONFIG_APPLY_WAITING) {
    out->general.node_id = svc->revert_to.node_id;
    out->link.magic      = svc->revert_to.magic;
    out->radio           = svc->revert_to.radio;
  }
}

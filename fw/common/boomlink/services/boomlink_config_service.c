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
  return magic <= 0xFFu;
}

static bool handle_set(boomlink_config_service_t *svc, const boomlink_ConfigSetRequest *req,
                       boomlink_ConfigMessage *out_response) {
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
  boomlink_node_config_t next = svc->current;
  if (req->has_general) {
    next.general          = req->general;
    next.general.node_id  = svc->current.general.node_id; /* hazardous - restored below if changed */
  }
  if (req->has_link) {
    next.link       = req->link;
    next.link.magic = svc->current.link.magic; /* hazardous - restored below if changed */
  }
  if (req->has_detection) {
    next.detection = req->detection;
  }
  if (req->has_gnss) {
    next.gnss = req->gnss;
  }
  if (req->has_telemetry) {
    next.telemetry = req->telemetry;
  }
  /* RadioConfig is entirely hazardous - `next.radio` deliberately NOT
     touched here, only in the staged snapshot below. */

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
  respond_set(out_response, boomlink_ConfigSetResult_CONFIG_SET_RESULT_PENDING_CONFIRMATION,
             svc->current.config_version);
  return true;
}

bool boomlink_config_service_handle(void *user, const boomlink_dispatch_rx_info_t *rx,
                                    const boomlink_ConfigMessage *request,
                                    boomlink_ConfigMessage *out_response) {
  (void)rx;
  boomlink_config_service_t *svc = (boomlink_config_service_t *)user;
  if (svc == NULL || out_response == NULL || request == NULL) {
    return false;
  }

  switch (request->which_message) {
    case boomlink_ConfigMessage_get_request_tag:
      return handle_get(svc, &request->message.get_request, out_response);
    case boomlink_ConfigMessage_set_request_tag:
      return handle_set(svc, &request->message.set_request, out_response);
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
  svc->apply_started_at_ms      = now_ms;
  svc->apply_state              = BOOMLINK_CONFIG_APPLY_WAITING;
}

void boomlink_config_service_confirm_pending_apply(boomlink_config_service_t *svc) {
  if (svc == NULL || svc->apply_state != BOOMLINK_CONFIG_APPLY_WAITING) {
    return;
  }
  svc->apply_state = BOOMLINK_CONFIG_APPLY_IDLE;
}

void boomlink_config_service_poll(boomlink_config_service_t *svc, uint32_t now_ms) {
  if (svc == NULL || svc->apply_state != BOOMLINK_CONFIG_APPLY_WAITING) {
    return;
  }
  /* Unsigned subtraction is wrap-safe for any real elapsed span - see
     fw/common/boomlink/linkengine/boomlink_port.h's boomlink_elapsed_ms()
     for the full reasoning (not linked here: that would pull a services
     module into depending on the link engine for one line of arithmetic
     that is exactly as correct inlined). */
  uint32_t elapsed = (uint32_t)(now_ms - svc->apply_started_at_ms);
  if (elapsed < svc->confirm_window_ms) {
    return;
  }
  svc->current.general.node_id = svc->revert_to.node_id;
  svc->current.link.magic      = svc->revert_to.magic;
  svc->current.radio           = svc->revert_to.radio;
  svc->apply_state             = BOOMLINK_CONFIG_APPLY_IDLE;
}

void boomlink_config_service_get_config(const boomlink_config_service_t *svc,
                                        boomlink_node_config_t *out) {
  if (out == NULL) {
    return;
  }
  *out = (svc != NULL) ? svc->current : (boomlink_node_config_t){0};
}

/**
 ******************************************************************************
 * @file    boomlink_config_service.c
 ******************************************************************************
 */
#include "boomlink_config_service.h"

#include <string.h>

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

static bool hazard_equal(const boomlink_config_hazard_t *a, const boomlink_config_hazard_t *b) {
  return a->node_id == b->node_id && a->magic == b->magic &&
         memcmp(&a->radio, &b->radio, sizeof(a->radio)) == 0;
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

static bool handle_set(boomlink_config_service_t *svc, const boomlink_ConfigSetRequest *req,
                       boomlink_ConfigMessage *out_response) {
  if (req->expected_config_version != svc->current.config_version) {
    respond_set(out_response, boomlink_ConfigSetResult_CONFIG_SET_RESULT_VERSION_CONFLICT,
               svc->current.config_version);
    return true;
  }

  if (svc->apply_state != BOOMLINK_CONFIG_APPLY_IDLE) {
    respond_set(out_response, boomlink_ConfigSetResult_CONFIG_SET_RESULT_APPLY_IN_PROGRESS,
               svc->current.config_version);
    return true;
  }

  /* Apply every NON-hazardous field immediately - only node_id/magic/radio
     wait for confirmation. A submessage's presence (has_X) is "this group
     was included in the write", independent of whether any hazardous field
     inside it actually changed value; the two are checked separately below.

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

  boomlink_config_hazard_t requested_hazard = hazard_snapshot(&svc->current);
  if (req->has_general) {
    requested_hazard.node_id = req->general.node_id;
  }
  if (req->has_link) {
    requested_hazard.magic = req->link.magic;
  }
  if (req->has_radio) {
    requested_hazard.radio = req->radio;
  }

  boomlink_config_hazard_t current_hazard = hazard_snapshot(&svc->current);
  bool hazard_changed = !hazard_equal(&requested_hazard, &current_hazard);

  next.config_version = svc->current.config_version + 1u;
  svc->current         = next;

  if (!hazard_changed) {
    respond_set(out_response, boomlink_ConfigSetResult_CONFIG_SET_RESULT_OK,
               svc->current.config_version);
    return true;
  }

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

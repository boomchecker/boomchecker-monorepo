/**
 ******************************************************************************
 * @file    boomlink_system_service.c
 ******************************************************************************
 */
#include "boomlink_system_service.h"

#include <string.h>

void boomlink_system_service_init(boomlink_system_service_t *svc, const boomlink_system_identity_t *identity,
                                  const boomlink_system_ops_t *ops) {
  if (svc == NULL) {
    return;
  }
  *svc = (boomlink_system_service_t){0};
  if (identity != NULL) {
    svc->identity = *identity;
  }
  svc->ops = ops;
}

static bool handle_ping(const boomlink_Ping *ping, boomlink_SystemMessage *out_response) {
  out_response->which_message = boomlink_SystemMessage_pong_tag;
  boomlink_Pong *pong          = &out_response->message.pong;
  /* boomlink_Ping_payload_t and boomlink_Pong_payload_t are structurally
     identical (both PB_BYTES_ARRAY_T(192)) but Nanopb generates them as
     distinct types, one per field - a plain struct assignment between them
     does not compile. memcpy the bytes and size explicitly instead. */
  pong->payload.size = ping->payload.size;
  memcpy(pong->payload.bytes, ping->payload.bytes, ping->payload.size);
  return true;
}

/* Overwrites any earlier staged/armed wakeup with this request's own
   window_s/reply-to - see boomlink_system_service_handle()'s own doc for
   why restarting, not queueing or ignoring, is the intended behaviour for a
   second WakeupRequest. Clearing `wakeup_armed` here matters as much as
   setting `wakeup_staged`: without it, a WakeupRequest arriving after an
   earlier one already armed (boomlink_system_service_arm_wakeup() already
   ran) would leave the OLD armed deadline in place until poll() fired it,
   racing the new one instead of replacing it. */
static void handle_wakeup_request(boomlink_system_service_t *svc, const boomlink_dispatch_rx_info_t *rx,
                                  const boomlink_WakeupRequest *req) {
  svc->wakeup_staged   = true;
  svc->staged_window_s = req->window_s;
  svc->staged_reply_to = (rx != NULL) ? rx->source_id : 0u;
  svc->wakeup_armed    = false;
}

bool boomlink_system_service_handle(void *user, const boomlink_dispatch_rx_info_t *rx,
                                    const boomlink_SystemMessage *request,
                                    boomlink_SystemMessage *out_response) {
  boomlink_system_service_t *svc = (boomlink_system_service_t *)user;
  if (svc == NULL || request == NULL || out_response == NULL) {
    return false;
  }

  switch (request->which_message) {
    case boomlink_SystemMessage_ping_tag:
      return handle_ping(&request->message.ping, out_response);

    case boomlink_SystemMessage_wakeup_request_tag:
      handle_wakeup_request(svc, rx, &request->message.wakeup_request);
      return false;

    case boomlink_SystemMessage_wakeup_response_tag:
      if (svc->ops != NULL && svc->ops->on_wakeup_response != NULL) {
        svc->ops->on_wakeup_response(svc->ops->ctx, (rx != NULL) ? rx->source_id : 0u,
                                     &request->message.wakeup_response);
      }
      return false;

    default:
      return false;
  }
}

void boomlink_system_service_arm_wakeup(boomlink_system_service_t *svc, uint32_t now_ms,
                                        uint32_t random_u32) {
  if (svc == NULL || !svc->wakeup_staged) {
    return;
  }
  /* +1 so a window_s of 0 still has exactly one possible outcome (delay 0,
     "respond immediately" - see WakeupRequest.window_s's own doc) rather than
     a modulo by zero. */
  uint32_t window_ms = svc->staged_window_s * 1000u + 1u;
  uint32_t delay_ms  = random_u32 % window_ms;

  svc->wakeup_staged    = false;
  svc->wakeup_armed     = true;
  svc->wakeup_fire_at_ms = now_ms + delay_ms;
  svc->wakeup_reply_to  = svc->staged_reply_to;
}

bool boomlink_system_service_poll(boomlink_system_service_t *svc, uint32_t now_ms,
                                  boomlink_SystemMessage *out_message, uint32_t *out_reply_to) {
  if (svc == NULL || out_message == NULL || out_reply_to == NULL || !svc->wakeup_armed) {
    return false;
  }
  /* Wrap-safe the same way boomlink_config_service.c's window_elapsed() is -
     see that function's own comment for the full reasoning. */
  if ((uint32_t)(now_ms - svc->wakeup_fire_at_ms) > 0x7FFFFFFFu) {
    return false; /* fire_at_ms is still in the future */
  }

  out_message->which_message              = boomlink_SystemMessage_wakeup_response_tag;
  boomlink_WakeupResponse *resp            = &out_message->message.wakeup_response;
  resp->node_id                            = 0u; /* filled by the caller - see this
                                                     function's own header doc: this
                                                     service has no notion of "this
                                                     node's own link address", only
                                                     of identity (device type/fw
                                                     version) it was initialized
                                                     with. */
  resp->device_type                        = svc->identity.device_type;
  resp->fw_version_major                   = svc->identity.fw_version_major;
  resp->fw_version_minor                   = svc->identity.fw_version_minor;
  resp->fw_version_patch                   = svc->identity.fw_version_patch;
  *out_reply_to                            = svc->wakeup_reply_to;

  svc->wakeup_armed = false;
  return true;
}

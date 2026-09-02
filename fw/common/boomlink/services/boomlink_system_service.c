/**
 ******************************************************************************
 * @file    boomlink_system_service.c
 ******************************************************************************
 */
#include "boomlink_system_service.h"

#include <string.h>

/* boomlink_Ping_payload_t and boomlink_Pong_payload_t are structurally
   identical (both PB_BYTES_ARRAY_T(192) today, per nanopb/system.options)
   but Nanopb generates them as distinct types, one per field - see
   handle_ping()'s own comment for why that forces an explicit memcpy below
   instead of a plain struct assignment. This guards the assumption behind
   that memcpy: a future system.options change that gives Pong less room
   than Ping would make it a silent stack buffer overflow on every echoed
   Ping, caught by nothing else here - decoded input can never make
   ping->payload.size exceed Ping's OWN capacity, but nothing stops it
   exceeding Pong's if the two ever diverge. Cli.c's proto_selftest carries
   the identical guard for the same two types, for the same reason. */
_Static_assert(sizeof(((boomlink_Pong *)0)->payload.bytes) >= sizeof(((boomlink_Ping *)0)->payload.bytes),
               "Pong payload capacity must cover Ping's for handle_ping()'s echo to stay memory-safe");

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
  /* Clamped BEFORE the *1000 below, not after: an unclamped window_s above
     ~4,294,967 overflows a 32-bit `window_s * 1000u`, and review found this
     was reachable straight off an unauthenticated broadcast frame's
     window_s field with no CLI in between to have already range-checked it
     (a raw WakeupRequest, not just `wakeup <window_s>`, can carry any
     uint32_t). BOOMLINK_SYSTEM_WAKEUP_MAX_WINDOW_S's own doc has the full
     reasoning for the specific bound. */
  uint32_t window_s = (svc->staged_window_s > BOOMLINK_SYSTEM_WAKEUP_MAX_WINDOW_S)
                          ? BOOMLINK_SYSTEM_WAKEUP_MAX_WINDOW_S
                          : svc->staged_window_s;
  /* +1 so a window_s of 0 still has exactly one possible outcome (delay 0,
     "respond immediately" - see WakeupRequest.window_s's own doc) rather than
     a modulo by zero. Cannot overflow now that window_s is clamped above:
     the largest possible window_ms is
     BOOMLINK_SYSTEM_WAKEUP_MAX_WINDOW_S * 1000u + 1u, far under UINT32_MAX. */
  uint32_t window_ms = window_s * 1000u + 1u;
  uint32_t delay_ms  = random_u32 % window_ms;

  svc->wakeup_staged      = false;
  svc->wakeup_armed       = true;
  svc->wakeup_armed_at_ms = now_ms;
  svc->wakeup_delay_ms    = delay_ms;
  svc->wakeup_reply_to    = svc->staged_reply_to;
}

bool boomlink_system_service_poll(boomlink_system_service_t *svc, uint32_t now_ms,
                                  boomlink_SystemMessage *out_message, uint32_t *out_reply_to) {
  if (svc == NULL || out_message == NULL || out_reply_to == NULL || !svc->wakeup_armed) {
    return false;
  }
  /* Elapsed-since-armed, wrap-safe the same way boomlink_config_service.c's
     window_elapsed() actually is: `(uint32_t)(now_ms - started_at_ms) >=
     window_ms`, exact for any real span under 2^32 ms (boomlink_port.h's
     boomlink_elapsed_ms() doc) - a fundamentally different (and correct for
     the FULL range) technique from this function's original
     "store an absolute fire_at_ms, compare against a fixed 0x7FFFFFFF half-
     range" version, which review found was only correct for delay_ms below
     2^31: with window_s clamped to BOOMLINK_SYSTEM_WAKEUP_MAX_WINDOW_S above,
     delay_ms can never approach that limit regardless, but this form needs
     no such limit to already be correct. */
  if ((uint32_t)(now_ms - svc->wakeup_armed_at_ms) < svc->wakeup_delay_ms) {
    return false; /* the drawn delay has not elapsed yet */
  }

  *out_message                             = (boomlink_SystemMessage){0};
  out_message->which_message               = boomlink_SystemMessage_wakeup_response_tag;
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

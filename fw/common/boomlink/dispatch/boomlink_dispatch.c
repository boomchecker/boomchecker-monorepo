/**
 ******************************************************************************
 * @file    boomlink_dispatch.c
 ******************************************************************************
 */
#include "boomlink_dispatch.h"

#include <string.h>

#include "boomlink_codec.h"

void boomlink_dispatch_init(boomlink_dispatch_t *dispatch,
                            const boomlink_dispatch_handlers_t *handlers) {
  if (dispatch == NULL) {
    return;
  }
  dispatch->handlers = (handlers != NULL) ? *handlers : (boomlink_dispatch_handlers_t){0};
  dispatch->stats    = (boomlink_dispatch_stats_t){0};
}

/* Builds the response envelope's header (protocol_version + correlated
   request_id) so every request/response branch below does it identically -
   a handler that built its own header could get protocol_version wrong or
   forget the correlation; see boomlink_dispatch_process()'s doc for why
   this is done here rather than left to each handler. */
static void init_response_header(boomlink_Envelope *response, const boomlink_Envelope *request) {
  boomlink_envelope_init(response);
  response->header.request_id = request->header.request_id;
}

boomlink_dispatch_result_t boomlink_dispatch_process(boomlink_dispatch_t *dispatch,
                                                     const boomlink_dispatch_rx_info_t *rx,
                                                     const boomlink_Envelope *envelope) {
  boomlink_dispatch_result_t result = {0};
  if (dispatch == NULL || envelope == NULL) {
    return result;
  }

  switch (envelope->which_payload) {
    case boomlink_Envelope_detection_tag:
      dispatch->stats.detection_received++;
      if (dispatch->handlers.on_detection != NULL) {
        dispatch->handlers.on_detection(dispatch->handlers.on_detection_user, rx,
                                        &envelope->payload.detection);
      } else {
        dispatch->stats.unhandled++;
      }
      break;

    case boomlink_Envelope_telemetry_tag:
      dispatch->stats.telemetry_received++;
      if (dispatch->handlers.on_telemetry != NULL) {
        dispatch->handlers.on_telemetry(dispatch->handlers.on_telemetry_user, rx,
                                        &envelope->payload.telemetry);
      } else {
        dispatch->stats.unhandled++;
      }
      break;

    case boomlink_Envelope_command_tag:
      dispatch->stats.command_received++;
      if (dispatch->handlers.on_command != NULL) {
        init_response_header(&result.response, envelope);
        result.response.which_payload = boomlink_Envelope_command_tag;
        result.has_response = dispatch->handlers.on_command(
            dispatch->handlers.on_command_user, rx, &envelope->payload.command,
            &result.response.payload.command);
      } else {
        dispatch->stats.unhandled++;
      }
      break;

    case boomlink_Envelope_config_tag:
      dispatch->stats.config_received++;
      if (dispatch->handlers.on_config != NULL) {
        init_response_header(&result.response, envelope);
        result.response.which_payload = boomlink_Envelope_config_tag;
        result.has_response = dispatch->handlers.on_config(
            dispatch->handlers.on_config_user, rx, &envelope->payload.config,
            &result.response.payload.config);
      } else {
        dispatch->stats.unhandled++;
      }
      break;

    case boomlink_Envelope_system_tag:
      dispatch->stats.system_received++;
      if (dispatch->handlers.on_system != NULL) {
        init_response_header(&result.response, envelope);
        result.response.which_payload = boomlink_Envelope_system_tag;
        result.has_response = dispatch->handlers.on_system(
            dispatch->handlers.on_system_user, rx, &envelope->payload.system,
            &result.response.payload.system);
      } else {
        dispatch->stats.unhandled++;
      }
      break;

    default:
      /* which_payload == 0: no oneof branch set. Wire-valid (an Envelope
         with only a header decodes fine - boomlink_codec.h's own doc), but
         nothing to route; not folded into `unhandled` since that counter
         means "we know what this was and chose not to act on it", which is
         a different, more actionable signal than "this carried nothing at
         all". */
      dispatch->stats.no_payload++;
      break;
  }

  if (!result.has_response) {
    result.response = (boomlink_Envelope){0};
  }
  return result;
}

void boomlink_dispatch_get_stats(const boomlink_dispatch_t *dispatch,
                                 boomlink_dispatch_stats_t *out) {
  if (out == NULL) {
    return;
  }
  *out = (dispatch != NULL) ? dispatch->stats : (boomlink_dispatch_stats_t){0};
}

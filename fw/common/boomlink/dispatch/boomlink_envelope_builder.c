/**
 ******************************************************************************
 * @file    boomlink_envelope_builder.c
 ******************************************************************************
 */
#include "boomlink_envelope_builder.h"

#include "boomlink_codec.h"

void boomlink_build_detection_event(boomlink_Envelope *out, const boomlink_DetectionEvent *event) {
  if (out == NULL) {
    return;
  }
  boomlink_envelope_init(out);
  out->which_payload                          = boomlink_Envelope_detection_tag;
  out->payload.detection.which_message        = boomlink_DetectionMessage_event_tag;
  out->payload.detection.message.event        = (event != NULL) ? *event : (boomlink_DetectionEvent){0};
}

void boomlink_build_telemetry_report(boomlink_Envelope *out, const boomlink_TelemetryReport *report) {
  if (out == NULL) {
    return;
  }
  boomlink_envelope_init(out);
  out->which_payload                    = boomlink_Envelope_telemetry_tag;
  out->payload.telemetry.which_message  = boomlink_TelemetryMessage_report_tag;
  out->payload.telemetry.message.report = (report != NULL) ? *report : (boomlink_TelemetryReport){0};
}

void boomlink_build_system_message(boomlink_Envelope *out, const boomlink_SystemMessage *message) {
  if (out == NULL) {
    return;
  }
  boomlink_envelope_init(out);
  out->which_payload = boomlink_Envelope_system_tag;
  out->payload.system = (message != NULL) ? *message : (boomlink_SystemMessage){0};
}

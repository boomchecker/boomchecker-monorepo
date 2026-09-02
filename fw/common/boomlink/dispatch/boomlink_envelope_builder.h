/**
 ******************************************************************************
 * @file    boomlink_envelope_builder.h
 * @brief   Small helpers for building the one-way (no correlated response)
 *          Envelope payloads a node originates itself - detection events and
 *          telemetry reports. Command/Config responses are NOT here: the
 *          dispatcher builds those (boomlink_dispatch_process()'s doc), since
 *          a response's header must correlate with the request that caused
 *          it, which these one-way messages have no such thing to do.
 *
 *          Matches section 4's original sketch naming (App/protocol/
 *          envelope_builder.h/.c), relocated to fw/common/boomlink for the
 *          same host-testability reasoning as the dispatcher itself.
 ******************************************************************************
 */
#ifndef BOOMLINK_ENVELOPE_BUILDER_H
#define BOOMLINK_ENVELOPE_BUILDER_H

#include "envelope.pb.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Wraps `event` in a fresh Envelope (header initialized, request_id left 0
 *  - header.proto: "zero when unused, e.g. an unsolicited message"; a
 *  DetectionEvent is exactly that, nothing correlates a response to it). */
void boomlink_build_detection_event(boomlink_Envelope *out, const boomlink_DetectionEvent *event);

/** As above, for a periodic TelemetryReport. */
void boomlink_build_telemetry_report(boomlink_Envelope *out, const boomlink_TelemetryReport *report);

#ifdef __cplusplus
}
#endif

#endif /* BOOMLINK_ENVELOPE_BUILDER_H */

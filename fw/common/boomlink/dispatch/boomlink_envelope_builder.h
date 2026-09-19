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
#include "system.pb.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Wraps `event` in a fresh Envelope (header initialized, request_id left 0
 *  - header.proto: "zero when unused, e.g. an unsolicited message"; a
 *  DetectionEvent is exactly that, nothing correlates a response to it). */
void boomlink_build_detection_event(boomlink_Envelope *out, const boomlink_DetectionEvent *event);

/** As above, for a periodic TelemetryReport. */
void boomlink_build_telemetry_report(boomlink_Envelope *out, const boomlink_TelemetryReport *report);

/**
 * As above, for a `SystemMessage` a node originates itself outside the
 * ordinary synchronous dispatch response path - section 8.6's `WakeupRequest`
 * (broadcast by an operator, nothing to correlate a reply to) and its
 * `WakeupResponse` (sent later, after a randomly-drawn delay - never inside
 * the RX handling that received the request, so the dispatcher's own
 * response-building path never runs for it either; see
 * boomlink_system_service.h). `Ping`/`Pong` do NOT need this helper: both
 * stay on the ordinary synchronous request/response path
 * `boomlink_dispatch_process()` already builds correlated headers for, since
 * section 8.5 requires `MessageHeader.request_id` to correlate a `Pong` back
 * to its `Ping` - a guarantee only that path provides.
 *
 * `message` is copied by value, not referenced - safe to pass a stack local
 * that goes out of scope right after this call returns.
 */
void boomlink_build_system_message(boomlink_Envelope *out, const boomlink_SystemMessage *message);

#ifdef __cplusplus
}
#endif

#endif /* BOOMLINK_ENVELOPE_BUILDER_H */

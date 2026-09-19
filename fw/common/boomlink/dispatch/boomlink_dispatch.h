/**
 ******************************************************************************
 * @file    boomlink_dispatch.h
 * @brief   Routes a decoded Envelope (boomlink.md section 7) to per-domain
 *          handlers by `which_payload`, and builds a correlated response
 *          Envelope for the request/response groups (command, config).
 *
 *          Deviates from section 4's original sketch, which put a
 *          "protocol_dispatcher" under fw/bom-stm32node/App/protocol/ -
 *          the same reasoning that moved the link engine out of App/link/
 *          and into fw/common/boomlink/linkengine/ applies here: this
 *          module is target-agnostic C with no HAL/RadioLib dependency, so
 *          it belongs where it can be host-tested, with a thin App/
 *          call site wiring real actions (reboot, flash, sensors) in later.
 *
 *          Target-agnostic and Nanopb-dependent (unlike linkframe/linkengine,
 *          which must NOT depend on Nanopb - section 9): this module's whole
 *          job is decoding what those layers deliberately never look inside.
 ******************************************************************************
 */
#ifndef BOOMLINK_DISPATCH_H
#define BOOMLINK_DISPATCH_H

#include <stdbool.h>
#include <stdint.h>

#include "envelope.pb.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * RX metadata a handler may need but which never lives inside the decoded
 * Envelope itself (header.proto's own doc: "Services that need the sender
 * identity... receive it as RX metadata passed alongside the decoded
 * Envelope"). Mirrors boomlink_link_rx_fn's parameters - this is the next
 * layer down the same information flows through.
 *
 * `destination_id` in particular: boomlink_link.h documents it as "the ONLY
 * way a caller can learn" whether a frame was unicast or broadcast, since
 * that distinction is absent from the Protobuf payload itself. A service
 * that needs to refuse a dangerous action over broadcast (e.g. section
 * 8.3's Reboot, or a ConfigSet touching a hazardous field) cannot do so
 * without this field - carried here even though no handler in this PR
 * reads it yet, so Phase C's caller has it to pass through without first
 * having to extend this struct.
 */
typedef struct {
  uint32_t source_id;
  uint32_t destination_id;
  float    rssi_dbm;
  float    snr_db;
} boomlink_dispatch_rx_info_t;

/**
 * One-way handlers: DetectionMessage/TelemetryMessage have no response
 * message group in the protocol (boomlink.md section 8), so there is
 * nothing for the dispatcher to build or send back. `envelope` is only
 * valid for the duration of the call, matching boomlink_link_rx_fn's own
 * "copy what you need" contract for the same reason: it points at storage
 * boomlink_dispatch_process()'s own caller owns, not this module.
 */
typedef void (*boomlink_dispatch_detection_fn)(void *user, const boomlink_dispatch_rx_info_t *rx,
                                               const boomlink_DetectionMessage *msg);
typedef void (*boomlink_dispatch_telemetry_fn)(void *user, const boomlink_dispatch_rx_info_t *rx,
                                               const boomlink_TelemetryMessage *msg);

/**
 * Request/response handlers: fill `*out_response` and return true to have
 * boomlink_dispatch_process() send it back (header built automatically -
 * see that function's doc); return false for no response at all. Command
 * and Config both work this way; System does too (Ping/Pong), even though
 * no PR before this one has wired an automatic responder for it - the slot
 * exists so one can be, without this module needing to special-case Ping.
 */
typedef bool (*boomlink_dispatch_command_fn)(void *user, const boomlink_dispatch_rx_info_t *rx,
                                             const boomlink_CommandMessage *request,
                                             boomlink_CommandMessage *out_response);
typedef bool (*boomlink_dispatch_config_fn)(void *user, const boomlink_dispatch_rx_info_t *rx,
                                            const boomlink_ConfigMessage *request,
                                            boomlink_ConfigMessage *out_response);
typedef bool (*boomlink_dispatch_system_fn)(void *user, const boomlink_dispatch_rx_info_t *rx,
                                            const boomlink_SystemMessage *request,
                                            boomlink_SystemMessage *out_response);

/**
 * Every handler is optional (NULL = this build does not act on that group -
 * e.g. a non-gateway node with no on_telemetry still decodes and counts a
 * peer's telemetry report, per get_stats(), without needing to do anything
 * with it). One `user` pointer per handler rather than one shared pointer:
 * command/config/detection/telemetry/system are independent services in
 * practice (section 4's original App/services/ sketch), and forcing them to
 * share one context would couple services that have no reason to know about
 * each other.
 */
typedef struct {
  boomlink_dispatch_detection_fn on_detection;
  void                          *on_detection_user;
  boomlink_dispatch_telemetry_fn on_telemetry;
  void                          *on_telemetry_user;
  boomlink_dispatch_command_fn   on_command;
  void                          *on_command_user;
  boomlink_dispatch_config_fn    on_config;
  void                          *on_config_user;
  boomlink_dispatch_system_fn    on_system;
  void                          *on_system_user;
} boomlink_dispatch_handlers_t;

/** Section 9.10-style counters, for the same reason BoomLink keeps its own:
 *  a drop or an unhandled message is invisible unless something counts it. */
typedef struct {
  uint32_t detection_received;
  uint32_t telemetry_received;
  uint32_t command_received;
  uint32_t config_received;
  uint32_t system_received;
  /* which_payload was 0 (proto3's oneof-not-set default) - wire-valid (an
     Envelope with only a header decodes fine, per boomlink_codec.h's own
     doc) but nothing to route. */
  uint32_t no_payload;
  /* which_payload matched a known group but that group's handler is NULL. */
  uint32_t unhandled;
} boomlink_dispatch_stats_t;

typedef struct {
  boomlink_dispatch_handlers_t handlers;
  boomlink_dispatch_stats_t    stats;
} boomlink_dispatch_t;

/** Zeroes `stats`; `handlers` is copied by value (NULL fields are fine - see
 *  the type's own doc). */
void boomlink_dispatch_init(boomlink_dispatch_t *dispatch,
                            const boomlink_dispatch_handlers_t *handlers);

/**
 * Route one decoded Envelope. Builds a full response Envelope automatically
 * when a request/response handler returns true: `boomlink_envelope_init()`
 * plus `request_id` copied from `envelope->header.request_id` (header.proto:
 * "correlates a request with its response at the application level") - the
 * handler only ever fills in the payload, never the header, so a handler
 * cannot forget the correlation or get protocol_version wrong.
 *
 * Does not send anything: this module has no radio/link dependency by
 * design (see the file doc comment), so the caller is what turns a returned
 * response Envelope into bytes on the air (boomlink_encode_envelope() +
 * boomlink_link_send()). A dispatcher that sent its own responses would
 * need a send callback and would stop being testable as a pure function -
 * exactly the layering boomlink_link_poll()'s callbacks already keep clean
 * one level down.
 */
typedef struct {
  bool              has_response;
  boomlink_Envelope response; /* valid only if has_response */
} boomlink_dispatch_result_t;

boomlink_dispatch_result_t boomlink_dispatch_process(boomlink_dispatch_t *dispatch,
                                                     const boomlink_dispatch_rx_info_t *rx,
                                                     const boomlink_Envelope *envelope);

/** NULL-tolerant, like boomlink_link_get_stats() - see that function's doc
 *  for why (CLI/diagnostic call sites where a missing reading beats a fault). */
void boomlink_dispatch_get_stats(const boomlink_dispatch_t *dispatch,
                                 boomlink_dispatch_stats_t *out);

#ifdef __cplusplus
}
#endif

#endif /* BOOMLINK_DISPATCH_H */

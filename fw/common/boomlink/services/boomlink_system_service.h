/**
 ******************************************************************************
 * @file    boomlink_system_service.h
 * @brief   Section 8.5's Ping/Pong echo and section 8.6's fleet discovery
 *          (Wakeup/WakeupResponse) - the boomlink_dispatch_system_fn this
 *          firmware has never wired until now (on_system was NULL through
 *          every prior phase - see protocol_service.c's own history).
 *
 *          Wakeup does not fit boomlink_dispatch_system_fn's synchronous
 *          "fill out_response, return true to send it now" contract the way
 *          Ping/Pong does - section 8.6's whole point is that the reply goes
 *          out LATER, after a randomly-drawn delay, not inside the RX
 *          handling that received the request. This file therefore splits
 *          Wakeup across three calls instead of one:
 *
 *          1. boomlink_system_service_handle() (the dispatch entry) stages a
 *             received WakeupRequest's window_s/reply-to address and answers
 *             `has_response = false` - the dispatch contract's own
 *             documented case for "no response for now" - without deciding
 *             a delay itself, because it has no random source or clock (it
 *             is host-testable, target-agnostic C, same reason
 *             boomlink_config_service_handle() takes no now_ms either).
 *          2. boomlink_system_service_arm_wakeup(), called by the firmware
 *             glue immediately after dispatch, turns that staged request
 *             into an actual deadline using a caller-supplied `now_ms` and
 *             one caller-supplied random draw - the same "caller owns time
 *             and randomness, this file only owns the state machine" split
 *             boomlink_config_service_commit_pending_apply()/poll() already
 *             use for now_ms.
 *          3. boomlink_system_service_poll(), called periodically, reports
 *             when the drawn deadline has passed and fills the
 *             WakeupResponse to send - the firmware glue still owns actually
 *             encoding and transmitting it, since this file has no link
 *             engine handle either.
 *
 *          A SECOND WakeupRequest arriving while one is already staged or
 *          armed restarts the window with the new request's own window_s -
 *          see boomlink_system_service_handle()'s own doc for why.
 ******************************************************************************
 */
#ifndef BOOMLINK_SYSTEM_SERVICE_H
#define BOOMLINK_SYSTEM_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "boomlink_dispatch.h"
#include "system.pb.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Reports a WakeupResponse this node received (i.e. this node was the one
 * that broadcast the matching WakeupRequest - section 8.6's "collection
 * point" role for that exchange). Optional: NULL means responses are still
 * counted by boomlink_dispatch_process()'s own stats but nothing further is
 * done with their content - the same "a NULL handler only counts the
 * message" contract boomlink_dispatch_process() already documents for every
 * other optional handler.
 *
 * `source_id` is the responding node's real link-layer address - the
 * authoritative one, not `response->node_id` (which is the SAME value the
 * responding node reports of itself, carried explicitly rather than left
 * implicit, per this service's own WakeupResponse field doc in system.proto -
 * a caller that wants to cross-check the two is free to, but is not required
 * to, since nothing in this design can make them disagree in practice).
 */
typedef void (*boomlink_system_wakeup_response_fn)(void *ctx, uint32_t source_id,
                                                   const boomlink_WakeupResponse *response);

typedef struct {
  boomlink_system_wakeup_response_fn on_wakeup_response;
  void                              *ctx;
} boomlink_system_ops_t;

/**
 * This node's own identity for any WakeupResponse it sends - fixed for the
 * life of the service instance (a firmware's device type and build version
 * do not change at runtime), supplied once at boomlink_system_service_init()
 * rather than re-passed on every call the way boomlink_command_ops_t's
 * per-command callbacks are, since there is exactly one value of each, not
 * one per action.
 */
typedef struct {
  boomlink_DeviceType device_type;
  uint32_t            fw_version_major;
  uint32_t            fw_version_minor;
  uint32_t            fw_version_patch;
} boomlink_system_identity_t;

typedef struct {
  boomlink_system_identity_t   identity;
  const boomlink_system_ops_t *ops; /* may be NULL - see boomlink_system_ops_t's own doc */

  /* Set by boomlink_system_service_handle() on a WakeupRequest, consumed and
     cleared by boomlink_system_service_arm_wakeup(). Never both this AND
     wakeup_armed true at once - arm_wakeup() clears staged the moment it
     reads it. */
  bool     wakeup_staged;
  uint32_t staged_window_s;
  uint32_t staged_reply_to;

  /* Set by boomlink_system_service_arm_wakeup(), consumed and cleared by
     boomlink_system_service_poll() once due. */
  bool     wakeup_armed;
  uint32_t wakeup_fire_at_ms;
  uint32_t wakeup_reply_to;
} boomlink_system_service_t;

/**
 * `svc` may be uninitialized garbage on entry - every field is set here, none
 * are preserved. NULL `svc` is a no-op (nothing to initialize into).
 * `ops` may be NULL (see boomlink_system_ops_t's own doc); when non-NULL, the
 * pointee must outlive `svc`, the same lifetime contract boomlink_command_
 * service_handle()'s `ops` parameter already carries.
 */
void boomlink_system_service_init(boomlink_system_service_t *svc, const boomlink_system_identity_t *identity,
                                  const boomlink_system_ops_t *ops);

/**
 * A boomlink_dispatch_system_fn: register with `handlers.on_system =
 * boomlink_system_service_handle` and `handlers.on_system_user = &svc` (a
 * `boomlink_system_service_t *` - NOT const, unlike command/config service's
 * `void *user`, because Wakeup's staging above mutates `svc` from what would
 * otherwise be a read-only dispatch call).
 *
 * - `Ping` -> answers `Pong` with the payload echoed back verbatim, `true`
 *   (send it now) - section 8.5's ordinary immediate round trip, unchanged
 *   from what Ping/Pong has always specified.
 * - `WakeupRequest` -> stages `window_s`/`rx->source_id` into `svc` (see the
 *   struct's own doc) and answers `false` (no response now - section 8.6's
 *   whole point). If a stage or an armed wakeup already exists from an
 *   earlier request, this OVERWRITES it with the new window_s/reply-to -
 *   restarting the window, not queueing a second one - because a requester
 *   broadcasting a second WakeupRequest almost always means the first round
 *   did not hear back from enough nodes, and every receiving node (including
 *   ones already counting down) getting a fresh, unbiased draw against the
 *   new window is the more useful behaviour than leaving some nodes still
 *   committed to a round the requester has effectively abandoned.
 * - `WakeupResponse` -> calls `ops->on_wakeup_response` if set (see its own
 *   doc), answers `false` - a response to a response would have nothing to
 *   correlate against and nobody to send it to.
 *
 * Returns `false` (no response, nothing staged) if `request` holds none of
 * the three messages above, or if `user`/`request`/`out_response` is NULL.
 */
bool boomlink_system_service_handle(void *user, const boomlink_dispatch_rx_info_t *rx,
                                    const boomlink_SystemMessage *request,
                                    boomlink_SystemMessage *out_response);

/**
 * Turns a staged WakeupRequest (see boomlink_system_service_t's own doc)
 * into an armed deadline: `now_ms + (random_u32 % (staged_window_s * 1000 +
 * 1))` milliseconds - a plain modulo, not rejection-sampled, for the same
 * reason section 9.7's own tx_jitter_max_ms draw is: this is collision
 * avoidance among a handful of nodes, not cryptography, and the bias a
 * modulo introduces against a 32-bit source is not measurable at these
 * window sizes. `random_u32` is a single caller-supplied draw, not a
 * generator this file calls itself - it has no random source of its own,
 * the same "caller owns randomness" split described in this file's own
 * header doc.
 *
 * A no-op if nothing is staged (`svc->wakeup_staged` false) - safe to call
 * unconditionally after every boomlink_system_service_handle() invocation
 * rather than requiring the caller to check first. NULL `svc` is also a
 * no-op.
 */
void boomlink_system_service_arm_wakeup(boomlink_system_service_t *svc, uint32_t now_ms,
                                        uint32_t random_u32);

/**
 * True exactly once an armed wakeup's deadline has passed - `now_ms >=
 * boomlink_system_service_t`'s own `wakeup_fire_at_ms` - and every time
 * called again after that, until a NEW WakeupRequest re-arms one, at which
 * point it goes back to reporting false until the new deadline passes.
 *
 * On a `true` return, fills `*out_message` with a WakeupResponse built from
 * `svc`'s own stored identity (see boomlink_system_identity_t) - EXCEPT
 * `node_id`, left `0`: this service has no notion of "this node's own link
 * address" (it is host-testable, target-agnostic C with no link engine
 * handle - the same reason it takes `random_u32`/`now_ms` as parameters
 * rather than reading them itself). The caller MUST fill `node_id` in with
 * the node's real address (e.g. `link_service_node_id()`) before sending -
 * see WakeupResponse's own field doc in system.proto for why it is carried
 * explicitly at all rather than left implicit in the link frame header's
 * own `source_id`. Also clears the armed state - so a caller that misses
 * acting on one `true` return
 * (e.g. because encoding or sending failed downstream) does not get a
 * second chance at the identical response; section 8.6 accepts that
 * failure mode as a reasonable one for a fire-and-forget discovery reply,
 * the same posture this codebase already takes for a lost `link ping` or a
 * best-effort broadcast reply.
 *
 * `*out_reply_to` (never NULL when `svc` and `out_message` are not) receives
 * the address to send it to - the requester's own `source_id`, staged back
 * in boomlink_system_service_handle().
 *
 * Returns `false` and touches neither output if nothing is armed yet, the
 * deadline has not passed, or `svc`/`out_message`/`out_reply_to` is NULL.
 */
bool boomlink_system_service_poll(boomlink_system_service_t *svc, uint32_t now_ms,
                                  boomlink_SystemMessage *out_message, uint32_t *out_reply_to);

#ifdef __cplusplus
}
#endif

#endif /* BOOMLINK_SYSTEM_SERVICE_H */

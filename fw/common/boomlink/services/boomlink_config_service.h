/**
 ******************************************************************************
 * @file    boomlink_config_service.h
 * @brief   Section 8.2's ConfigGet/ConfigSet over an in-memory NodeConfig -
 *          persistence to flash (section 10.1) is Phase B's job, not this
 *          one's; this module only knows "the config right now", which is
 *          exactly what Phase B needs to load into and save out of.
 ******************************************************************************
 */
#ifndef BOOMLINK_CONFIG_SERVICE_H
#define BOOMLINK_CONFIG_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "boomlink_dispatch.h"
#include "config.pb.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Section 8.2's NodeConfig tree, plus the version stamp optimistic
 *  concurrency needs. */
typedef struct {
  uint32_t                  config_version;
  boomlink_GeneralConfig    general;
  boomlink_LinkConfig       link;
  boomlink_RadioConfig      radio;
  boomlink_DetectionConfig  detection;
  boomlink_GnssConfig       gnss;
  boomlink_TelemetryConfig  telemetry;
} boomlink_node_config_t;

/** Safe defaults (section 10.1's "missing/invalid -> load safe defaults"),
 *  for both a genuinely fresh node and Phase B's corrupt-storage fallback.
 *  `config_version` starts at 1 - 0 is reserved the same way session_id 0
 *  is elsewhere in this codebase: the value an unseeded/zeroed struct would
 *  produce by accident, not a value any real config should carry. */
void boomlink_node_config_defaults(boomlink_node_config_t *out);

/**
 * section 8.2's revert-on-timeout apply, generalized to every field that
 * paragraph names as needing it: RadioConfig (its own literal example) and
 * GeneralConfig.node_id / LinkConfig.magic (the "two fields need the same
 * hazard treatment" paragraph). Exactly one hazardous change may be in
 * flight at a time - see CONFIG_SET_RESULT_APPLY_IN_PROGRESS.
 */
typedef enum {
  BOOMLINK_CONFIG_APPLY_IDLE = 0,   /* no hazardous change in flight */
  BOOMLINK_CONFIG_APPLY_STAGED,     /* response built; not yet told the send happened */
  BOOMLINK_CONFIG_APPLY_WAITING,    /* applied; waiting for confirmation or timeout */
} boomlink_config_apply_state_t;

/** The subset of NodeConfig that needs revert-on-timeout, as one atomic
 *  unit - section 8.2 talks about a radio profile change as a single
 *  all-or-nothing apply, and node_id/magic get "the same hazard treatment",
 *  not a separate one each. */
typedef struct {
  uint32_t             node_id;
  uint32_t             magic;
  boomlink_RadioConfig radio;
} boomlink_config_hazard_t;

typedef struct {
  boomlink_node_config_t        current;

  boomlink_config_apply_state_t apply_state;
  boomlink_config_hazard_t      staged;      /* new values, not yet in `current` */
  boomlink_config_hazard_t      revert_to;   /* pre-change values, restored on timeout */
  /* WAITING: the real boomlink_config_service_commit_pending_apply() time,
     start of the confirm-window countdown to a revert. STAGED: latched by
     boomlink_config_service_poll() the first time it observes the stage
     (handle_set() has no clock of its own - its signature is fixed by
     boomlink_dispatch_config_fn), start of a DIFFERENT countdown to
     abandoning a stage nobody ever committed. Never both at once - see
     boomlink_config_service_poll()'s doc. */
  uint32_t                      apply_started_at_ms;
  bool                          staged_seen; /* has poll() latched apply_started_at_ms for STAGED yet */
  uint32_t                      confirm_window_ms;
} boomlink_config_service_t;

/** `initial` is copied in as-is - the caller (Phase B, once it exists) has
 *  already resolved "load stored config or fall back to
 *  boomlink_node_config_defaults()" before this is called; this module
 *  does not know or care which happened. `confirm_window_ms` is section
 *  8.2's revert-on-timeout window length - a policy choice with no spec-
 *  given default, left to the caller rather than hidden in here. */
void boomlink_config_service_init(boomlink_config_service_t *svc,
                                  const boomlink_node_config_t *initial,
                                  uint32_t confirm_window_ms);

/**
 * A boomlink_dispatch_config_fn: register with `handlers.on_config =
 * boomlink_config_service_handle` and `handlers.on_config_user = svc`.
 *
 * A GET always answers (returns true). A SET answers OK/VERSION_CONFLICT/
 * INVALID/APPLY_IN_PROGRESS immediately and applies non-hazardous fields
 * right away; a hazardous field change answers PENDING_CONFIRMATION and
 * STAGES rather than applies (see boomlink_config_service_commit_pending_
 * apply()'s doc for why applying here would be too early). Returns false
 * only if `request` carries neither a GetRequest nor a SetRequest, or
 * `user`/`out_response` is NULL.
 */
bool boomlink_config_service_handle(void *user, const boomlink_dispatch_rx_info_t *rx,
                                    const boomlink_ConfigMessage *request,
                                    boomlink_ConfigMessage *out_response);

/**
 * Call once the caller KNOWS the PENDING_CONFIRMATION response this service
 * built has actually been transmitted (e.g. the link layer reports the
 * frame carrying it sent, or - for a transport with no such signal -
 * immediately). No-op unless apply_state == STAGED.
 *
 * Why this cannot happen inside boomlink_config_service_handle() itself:
 * section 8.2 requires the hazardous change is "not applied before the
 * response confirming the change has been transmitted" - and this function
 * returns the response to a caller that still has to encode and send it.
 * Applying here would switch the radio/identity out from under a response
 * this same call is still trying to get onto the air, the same ordering
 * hazard boomlink_command_service.h's reboot() doc describes for a
 * synchronous reset.
 *
 * Moves `staged` into `current`, starts the confirmation window from
 * `now_ms` (overwriting whatever boomlink_config_service_poll() may have
 * latched into apply_started_at_ms while STAGED - see that function's own
 * doc), and moves to WAITING.
 */
void boomlink_config_service_commit_pending_apply(boomlink_config_service_t *svc, uint32_t now_ms);

/**
 * Call when the caller has positively confirmed the new profile/identity
 * actually works - what "confirmed" means is a Phase C/integration decision
 * this module deliberately does not make (section 8.2 names the requirement
 * - "a confirmation exchange on the new profile" - without naming the exact
 * message, and guessing one here would be inventing protocol the spec does
 * not define). No-op unless apply_state == WAITING. Moves to IDLE; the
 * change is now permanent.
 */
void boomlink_config_service_confirm_pending_apply(boomlink_config_service_t *svc);

/**
 * Call periodically (every superloop tick, like boomlink_link_poll()).
 *
 * WAITING: reverts `current`'s hazardous fields to `revert_to` (and bumps
 * `config_version`, so a client's cached version can no longer match a node
 * whose state just changed underneath it) if confirm_window_ms has elapsed
 * since boomlink_config_service_commit_pending_apply().
 *
 * STAGED: if the caller never learns the PENDING_CONFIRMATION response was
 * actually transmitted - a send failure, a crash, a bug - and so never
 * calls boomlink_config_service_commit_pending_apply(), apply_state would
 * otherwise stay STAGED forever, and every later hazardous SET would answer
 * APPLY_IN_PROGRESS permanently: the node's configuration becomes
 * unwritable until reboot, including the write that would fix whatever
 * went wrong. To close that without giving handle_set() a clock it does not
 * have (its signature is fixed by boomlink_dispatch_config_fn), this
 * function itself latches `apply_started_at_ms` the FIRST time it observes
 * STAGED, then abandons the stage once confirm_window_ms has elapsed since
 * that latch - nothing in `current` was ever touched by staging alone, so
 * there is nothing to revert, only the stage itself to drop.
 *
 * IDLE: no-op.
 */
void boomlink_config_service_poll(boomlink_config_service_t *svc, uint32_t now_ms);

/** NULL-tolerant, matching this codebase's other diagnostics accessors. */
void boomlink_config_service_get_config(const boomlink_config_service_t *svc,
                                        boomlink_node_config_t *out);

#ifdef __cplusplus
}
#endif

#endif /* BOOMLINK_CONFIG_SERVICE_H */

/**
 ******************************************************************************
 * @file    protocol_service.h
 * @brief   PR 4 Phase C's call site: wires fw/common/boomlink's protocol
 *          dispatcher (boomlink_dispatch.h) and its command/config services
 *          onto the link engine App/link/link_service.h already brings up,
 *          and owns the boot-time NodeConfig load those services and
 *          link_service_init() both need.
 *
 *          Deliberately NOT the dispatcher/services themselves - those stay
 *          in fw/common/boomlink so they remain host-testable (see
 *          boomlink_dispatch.h's own doc on why it moved out of this
 *          directory's original sketch location). This file is the thin,
 *          target-specific remainder: real actions for the command service
 *          (radio.h, HAL_NVIC_SystemReset), the real flash-backed config
 *          load/save, and the send-the-response half of a request/response
 *          exchange that boomlink_dispatch_process() deliberately leaves to
 *          its caller.
 ******************************************************************************
 */
#ifndef PROTOCOL_SERVICE_H
#define PROTOCOL_SERVICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "boomlink_config_service.h"
#include "system.pb.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * This firmware's own build version, reported in a section 8.6
 * WakeupResponse (boomlink_system_identity_t) - the single source of truth
 * `cli.c`'s `version` command now also reads from, rather than each
 * carrying its own separately-maintained literal. Bump by hand for now;
 * nothing in this build wires it to git-describe or a CI-stamped value, the
 * same "reasonable by inspection, not yet tuned" caveat this codebase's
 * other bring-up-only constants already carry.
 */
#define PROTOCOL_SERVICE_FW_VERSION_MAJOR 0u
#define PROTOCOL_SERVICE_FW_VERSION_MINOR 1u
#define PROTOCOL_SERVICE_FW_VERSION_PATCH 0u

/**
 * Load this node's persisted configuration - boomlink_config_store_load()
 * against App/storage/boomlink_flash_storage_port.h's real flash region,
 * falling back to boomlink_node_config_defaults() on a missing/invalid save
 * (section 10.1's "missing/invalid -> load safe defaults"; boomlink_config_
 * store_load() does not distinguish which, so neither does this).
 *
 * Call once, from cli_init(), BEFORE link_service_init(): `out->general.
 * node_id` and `out->link.magic` are what cli_init() then passes to
 * link_service_init() (section 7.2/7.3's identity, sourced from config now
 * that this exists - see link_service_init()'s own doc for the fallback it
 * still applies on top of whatever this returns). Pass the SAME `out` on to
 * protocol_service_init() afterwards - this function does not cache it, and
 * loading twice would risk observing two different answers if a save
 * happened to land in between (protocol_service_on_rx() does call
 * boomlink_config_store_save() once this node is up and answering requests
 * - see persist_current_config() - but nothing calls it before boot-time
 * loading finishes, so this still cannot happen at THIS call site; this
 * function does not rely on that either way).
 */
void protocol_service_load_config(boomlink_node_config_t *out);

/**
 * Wire the protocol dispatcher (command + config services) against
 * `loaded_config`. Call once, from cli_init(), right after link_service_
 * init() - `loaded_config` must be what protocol_service_load_config()
 * returned, with exactly one field cli.c is expected to have corrected
 * first: `general.node_id` set to `link_service_node_id()`'s result (the
 * value link_service_init() actually resolved and is now running with -
 * see that function's own doc for when this differs from what was loaded).
 * Skipping that correction is not a wiring error this function can detect
 * or reject; it just means ConfigGet reports 0x00000000 ("unconfigured")
 * for the address this node is demonstrably alive and answering requests
 * on - exactly the state every never-configured node starts in, since
 * boomlink_node_config_defaults() leaves node_id at that same sentinel.
 *
 * Registers no callback of its own with link_service_init(): cli.c's
 * existing link_on_rx() (already link_service_init()'s on_rx) calls
 * protocol_service_on_rx() itself, after its own debug preview - see that
 * function's doc and boomlink_link_rx_fn's own doc comment, which already
 * names this exact call site.
 *
 * @return false if `loaded_config` is NULL. The service then stays
 *         uninitialized - protocol_service_on_rx()/_process() become no-ops
 *         - matching link_service_init()'s own "no bring-up retry" contract
 *         on failure.
 */
bool protocol_service_init(const boomlink_node_config_t *loaded_config);

/**
 * link_service_init()'s on_rx, reached via cli.c's link_on_rx(). Decodes
 * `payload` as an Envelope, routes it through boomlink_dispatch_process(),
 * and - if that produces a response - encodes and sends it back to
 * `source_id` over the link engine (App/link/link_service.h's
 * link_service_link()).
 *
 * Also where a config change this node accepts actually reaches flash: any
 * accepted ConfigSet's already-applied non-hazardous fields are persisted
 * immediately (regardless of send success, and whether the overall result
 * is OK or PENDING_CONFIRMATION), and a hazardous field is persisted again
 * once its revert-on-timeout window closes with a real confirmation - see
 * this function's own body comments for exactly which points persist and
 * why not any of the others (commit/abandon/revert never do).
 *
 * A boomlink_link_rx_fn "cannot fail" (that typedef's own doc): a payload
 * that fails to decode, a dispatch that produces no response, an encode
 * that fails, or a link_service_link() not yet available are all silent
 * no-ops here, not errors this function has any way to report - cli.c's own
 * link_on_rx() debug preview is what still gives an operator visibility
 * into what actually reached the radio, decoded or not. Also a no-op if
 * protocol_service_init() has not (yet, or ever) succeeded.
 */
void protocol_service_on_rx(void *user, uint32_t source_id, uint32_t destination_id,
                            const uint8_t *payload, size_t payload_len,
                            float rssi_dbm, float snr_db);

/**
 * Call every superloop iteration, alongside link_service_process() (from
 * cli_process()). Drives boomlink_config_service_poll()'s revert-on-timeout
 * and STAGED-abandon, and performs the deferred HAL_NVIC_SystemReset() a
 * `reboot` command armed, once its delay has elapsed - see boomlink_
 * command_service.h's reboot() doc for why the reset itself cannot happen
 * synchronously inside that command's own callback. No-op if protocol_
 * service_init() has not succeeded.
 */
void protocol_service_process(void);

/**
 * Reports a section 8.6 WakeupResponse this node received - i.e. this node
 * was the one that broadcast the matching WakeupRequest (the `wakeup`
 * CLI command, cli.c's cmd_wakeup()). NULL is the default and a valid
 * choice: responses are still counted by boomlink_dispatch_process()'s own
 * stats either way, matching every other optional dispatch handler's "a
 * NULL handler only counts the message" contract.
 *
 * Registered once, from cli_init() after protocol_service_init() - it is
 * cli.c that owns printing to the operator's console (embeddedCliPrint(),
 * `s_cli`), the same reason link_service_init() takes its own on_rx/
 * on_tx_done callbacks FROM cli.c rather than this file printing directly;
 * protocol_service.c has never had a CLI-facing print path of its own
 * before this, and gaining one now would blur that layering rather than
 * reuse it.
 */
typedef void (*protocol_service_wakeup_response_fn)(uint32_t source_id,
                                                     const boomlink_WakeupResponse *response);
void protocol_service_set_wakeup_response_callback(protocol_service_wakeup_response_fn cb);

/**
 * Broadcast a section 8.6 WakeupRequest asking every reachable node to
 * answer within `window_s` seconds, each after its own randomly-drawn
 * delay - see boomlink.md section 8.6 and boomlink_system_service.h for the
 * full mechanism this triggers. No-op if protocol_service_init() has not
 * succeeded. Broadcast with no ACK requested, the same
 * `destination_id != BOOMLINK_ADDR_BROADCAST` reasoning
 * protocol_service_on_rx() already applies to every broadcast-addressed
 * reply it sends - an ACK from every reachable node at once is exactly the
 * "N simultaneous responses" problem that reasoning already exists to
 * avoid, and nothing here waits for one anyway.
 */
void protocol_service_send_wakeup_request(uint32_t window_s);

#ifdef __cplusplus
}
#endif

#endif /* PROTOCOL_SERVICE_H */

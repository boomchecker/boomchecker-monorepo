/**
 ******************************************************************************
 * @file    link_service.h
 * @brief   The link engine's one call site (docs/firmware/bom-stm32node/
 *          boomlink.md section 4's "not four files but one call site"):
 *          owns the boomlink_link_t instance, its config and its radio port,
 *          and gives cli.c what it needs to drive and observe it.
 ******************************************************************************
 */
#ifndef LINK_SERVICE_H
#define LINK_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "boomlink_link.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Bring the link engine up against radio.h. Call once, from cli_init() -
 * after main()'s radio_init(), which is what makes the node_id/session_id
 * derivation below meaningful, though not calling order that matters
 * mechanically: the port's callbacks forward to radio.h's singleton on every
 * call rather than caching anything, so bringing this up before the radio is
 * ready would only mean every send/poll is a harmless no-op until it is.
 *
 * on_rx/on_tx_done/their user pointers become the engine's config (see
 * boomlink_link_rx_fn/boomlink_link_tx_done_fn in boomlink_link.h); either
 * may be NULL.
 *
 * @return false if boomlink_link_init() itself refused the config or port
 *         (see its own doc for why - not expected to ever happen with this
 *         file's own fixed config, but not asserted away either). The
 *         service then stays uninitialized: no bring-up retry, matching
 *         radio_init()'s own once-at-boot contract.
 */
bool link_service_init(boomlink_link_rx_fn on_rx, void *on_rx_user,
                       boomlink_link_tx_done_fn on_tx_done, void *on_tx_done_user);

/**
 * Service the link engine - drains RX, then services TX (boomlink_link_poll())
 * - if link_service_init() succeeded and the service is enabled (see
 * link_service_set_enabled()). Call every superloop iteration, from
 * cli_process(), never from interrupt context - the same constraint
 * boomlink_link_poll() and radio_process() already carry.
 */
void link_service_process(void);

/**
 * Whether the link engine currently owns radio_poll_rx(). radio.h's poll is
 * single-consumer by its own doc: on any tick, exactly one of
 * link_service_process() and cli.c's raw RX preview may call it, which is
 * what this flag arbitrates. Defaults to true.
 */
bool link_service_enabled(void);

/**
 * Hand ownership of radio_poll_rx() to (`true`) or away from (`false`) the
 * link engine - `link enable`/`link disable` in cli.c. Disabling stops
 * link_service_process() from touching the radio AT ALL, not just its RX
 * side: an operator asking for the raw radio back (boomlink.md section
 * 15.3's PHY-only bring-up test) gets it back completely, not a half-paused
 * engine still trying to complete a pending send underneath them.
 */
void link_service_set_enabled(bool enabled);

/**
 * This node's address (section 7.2), for `link status` and for an operator
 * to read off and hand to a peer's `link ping <node_id>`. Derived once at
 * link_service_init() - see the .c file for how and why, and the documented
 * bring-up-only nature of that choice. 0 before a successful
 * link_service_init().
 */
uint32_t link_service_node_id(void);

/**
 * The engine instance itself, for cli.c to call boomlink_link_send() and the
 * read-only diagnostics (boomlink_link_get_stats(), _tx_state(),
 * _queue_depth(), _session_id()) directly rather than this file re-wrapping
 * each one for no reason. NULL before a successful link_service_init() - all
 * four diagnostics are NULL-tolerant (three say so in their own doc comment;
 * _queue_depth()'s doesn't mention NULL but its implementation is - checked,
 * not assumed). send() and poll() are NOT NULL-tolerant, but cli.c never
 * calls them without checking this accessor's result first.
 */
boomlink_link_t *link_service_link(void);

#ifdef __cplusplus
}
#endif

#endif /* LINK_SERVICE_H */

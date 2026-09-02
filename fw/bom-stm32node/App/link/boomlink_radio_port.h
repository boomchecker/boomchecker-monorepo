/**
 ******************************************************************************
 * @file    boomlink_radio_port.h
 * @brief   boomlink_port.h's seam, implemented against App/radio/radio.h -
 *          the only radio this firmware has, so this adapter is a thin
 *          forward with no state of its own beyond the PRNG boomlink_port_t's
 *          random_u32 needs (radio.h has none, and enabling STM32H5's RNG
 *          peripheral just for this is more than a collision-avoidance
 *          source needs - see the .c file).
 ******************************************************************************
 */
#ifndef BOOMLINK_RADIO_PORT_H
#define BOOMLINK_RADIO_PORT_H

#include "boomlink_port.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Fill `*out` with a boomlink_port_t backed by radio.h. Call once, after
 * radio_init() has returned successfully - the port's callbacks forward to
 * radio.h's singleton on every call rather than caching anything from it, so
 * calling this before radio_init() would not be wrong, only pointless: the
 * port would just forward to a not-yet-ready radio until it is.
 *
 * `out->ctx` is left NULL: radio.h is a global singleton with no instance to
 * point at, which is exactly the case boomlink_port.h's own comment names as
 * the reason a fake test port needs a real ctx and this one does not.
 */
void boomlink_radio_port_init(boomlink_port_t *out);

/**
 * One draw from this port's own per-node PRNG - the same generator and state
 * boomlink_port_t.random_u32 forwards to for the link engine's own retry/
 * jitter backoff (section 9.7). Exposed so a caller outside the link engine
 * (section 8.6's fleet-discovery wakeup delay, drawn at a much larger
 * timescale than section 9.7's own jitter - see boomlink_system_service.h)
 * can draw genuine per-node randomness without this firmware standing up a
 * second, differently-seeded generator for the same "quality requirements
 * are low, not cryptography" need boomlink_port.h's own random_u32 doc
 * already describes.
 *
 * Only meaningful after boomlink_radio_port_init() has run (the PRNG is
 * seeded there, from this chip's factory UID) - calling this first returns a
 * draw from the unseeded, deterministic zero-initialized state, the same
 * "pointless, not wrong" caveat boomlink_radio_port_init()'s own doc gives
 * for calling before radio_init().
 *
 * NOT ISR-safe: next_u32() (the .c file) is a plain, non-atomic read-modify-
 * write on file-static state, safe today only because every caller - the
 * link engine's own port callback and this function's callers alike - runs
 * from the main superloop (boomlink_link_poll()/_process(), never an
 * interrupt handler). This accessor's whole purpose is inviting callers
 * OUTSIDE the link engine to reach the same PRNG, so this is worth stating
 * explicitly rather than leaving it as an assumption only the link engine's
 * own call site happened to already satisfy.
 */
uint32_t boomlink_radio_port_random_u32(void);

#ifdef __cplusplus
}
#endif

#endif /* BOOMLINK_RADIO_PORT_H */

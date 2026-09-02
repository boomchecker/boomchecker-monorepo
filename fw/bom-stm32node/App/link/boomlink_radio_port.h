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

#ifdef __cplusplus
}
#endif

#endif /* BOOMLINK_RADIO_PORT_H */

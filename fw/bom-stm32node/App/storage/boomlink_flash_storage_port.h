/**
 ******************************************************************************
 * @file    boomlink_flash_storage_port.h
 * @brief   boomlink_storage_port_t's seam, implemented against the last 16K
 *          flash sector this firmware's linker script reserves for it
 *          (STM32H563xx_FLASH.ld, PR 4 Phase B).
 ******************************************************************************
 */
#ifndef BOOMLINK_FLASH_STORAGE_PORT_H
#define BOOMLINK_FLASH_STORAGE_PORT_H

#include "boomlink_storage_port.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Fill `*out` with a boomlink_storage_port_t backed by this chip's reserved
 * config sector. Safe to call at any point in boot - unlike
 * boomlink_radio_port_init(), which only becomes meaningful once radio_init()
 * has run, flash is addressable from reset and this port does not talk to
 * any peripheral that needs its own init first.
 *
 * `out->ctx` is left NULL: like boomlink_radio_port.c, this chip's flash
 * controller is a singleton with no instance to point at.
 */
void boomlink_flash_storage_port_init(boomlink_storage_port_t *out);

#ifdef __cplusplus
}
#endif

#endif /* BOOMLINK_FLASH_STORAGE_PORT_H */

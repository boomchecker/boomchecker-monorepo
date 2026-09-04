/**
 ******************************************************************************
 * @file    dfu_boot.h
 * @brief   Software entry into the STM32H5 ROM bootloader (USB DFU).
 *
 * Lets the board be reflashed over the user USB connector without an ST-Link:
 * the `dfu` console command detaches from the bus and jumps into system
 * memory, after which the ROM bootloader enumerates as "STM32 BOOTLOADER"
 * and STM32CubeProgrammer can flash with `-c port=USB1`.
 ******************************************************************************
 */
#ifndef DFU_BOOT_H
#define DFU_BOOT_H

/** Detach USB, de-init clocks and jump to the ROM bootloader. Never returns. */
void dfu_boot_enter(void);

#endif /* DFU_BOOT_H */

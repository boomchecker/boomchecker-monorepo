/**
 ******************************************************************************
 * @file    usb_cli.h
 * @brief   Bridge between the CLI (cli.c) and USBX CDC ACM (standalone mode).
 *
 * Connects the transport-agnostic cli.c to the USBX device CDC ACM class.
 * Output goes through the stateful ux_device_class_cdc_acm_write_run, input
 * through ..._read_run, all driven from usb_cli_process() in the main loop
 * (no RTOS).
 ******************************************************************************
 */
#ifndef USB_CLI_H
#define USB_CLI_H

#include <stdint.h>

/** Create the CLI and bind it to the USB CDC transport. Call after MX_USBX_Init(). */
void usb_cli_init(void);

/** Main-loop pump: drives the USBX stack, the CLI and CDC read/write. */
void usb_cli_process(void);

/** CDC ACM class callbacks (call from ux_device_cdc_acm.c USER CODE blocks). */
void usb_cli_on_activate(void *cdc_acm_instance);
void usb_cli_on_deactivate(void);

#endif /* USB_CLI_H */

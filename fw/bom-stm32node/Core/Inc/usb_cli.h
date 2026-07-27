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
#include <stdbool.h>

/** Bring up the USB CDC console: configure endpoint packet memory (PMA), connect
 *  the device to the bus (HAL_PCD_Start) and bind the CLI to the CDC transport.
 *  Call once after MX_USBX_Init(). */
void usb_cli_start(void);

/** Main-loop pump: drives the USBX stack, the CLI and CDC read/write. */
void usb_cli_process(void);

/** CDC ACM class callbacks (call from ux_device_cdc_acm.c USER CODE blocks). */
void usb_cli_on_activate(void *cdc_acm_instance);
void usb_cli_on_deactivate(void);

/** @return true while the CDC ACM class is configured (host connected). */
bool usb_cli_connected(void);

/** Service the USBX stack once without touching the CLI. Use it inside a
 *  blocking loop (e.g. waiting for a PCM block) to keep the device alive. */
void usb_cli_pump(void);

/** Push any pending console text out to the host synchronously. Call before a
 *  binary transfer so the CLI's text (command echo) does not interleave with
 *  the payload.
 *  @return true if the pipe is idle after the flush, false on timeout/error. */
bool usb_cli_flush_tx(void);

/** Send `len` bytes losslessly, driving the CDC write state machine to
 *  completion (unlike the drop-on-full console path). Blocks until every byte
 *  is accepted, the host stalls past a timeout, or the device disconnects.
 *  @return 0 on success, non-zero on timeout/disconnect. */
int usb_cli_write_blocking(const uint8_t *data, uint32_t len);

/** Non-blocking counterpart of usb_cli_write_blocking, for pipelined streaming:
 *  one block is transmitted while the next is acquired.
 *
 *  usb_cli_write_start()  - begin sending `len` bytes from `data` (which must
 *                           stay valid until the send finishes). Returns 0 if
 *                           accepted, non-zero if busy/not connected.
 *  usb_cli_write_service()- advance the send. 0 = done/idle, 1 = still busy,
 *                           <0 = error/disconnect. Call it while doing other work.
 *  usb_cli_write_active() - true until the started send completes. */
int  usb_cli_write_start(const uint8_t *data, uint32_t len);
int  usb_cli_write_service(void);
bool usb_cli_write_active(void);

/** Abort a started (or stalled) non-blocking send and reset the underlying CDC
 *  write state machine so the next write starts from a clean state. */
void usb_cli_write_abort(void);

#endif /* USB_CLI_H */

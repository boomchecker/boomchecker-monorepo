/**
 ******************************************************************************
 * @file    usb_cli.c
 * @brief   CLI over USBX CDC ACM (standalone). Bridges cli.c to USBX.
 ******************************************************************************
 */
#include "usb_cli.h"
#include "cli.h"

#include "ux_api.h"
#include "ux_device_class_cdc_acm.h"

#include <string.h>

/* CDC ACM class instance, set on activate / cleared on deactivate. */
static UX_SLAVE_CLASS_CDC_ACM *s_cdc;

/* RX scratch: one bulk-OUT packet (FS wMaxPacketSize = 64). */
static uint8_t s_rx[64];

/* TX staging for one in-flight chunk. Must hold the largest contiguous chunk
   cli.c can hand us in a single cli_tx callback (i.e. its whole TX ring), so a
   chunk is never partially accepted and lost. */
static uint8_t  s_tx_buf[512];
static uint16_t s_tx_len;
static uint8_t  s_tx_busy;

/* cli_tx_fn: stage one chunk for transmission. The actual USB write is driven
   in usb_cli_process(). Returns 0 if accepted, non-zero if busy (cli.c keeps
   the bytes and retries later). */
static int usb_cli_tx(const uint8_t *buf, uint16_t len)
{
  if (s_cdc == UX_NULL || s_tx_busy)
  {
    return 1; /* not connected yet, or previous chunk still in flight */
  }
  if (len > sizeof(s_tx_buf))
  {
    len = sizeof(s_tx_buf); /* cli TX ring <= s_tx_buf, so this never truncates */
  }
  memcpy(s_tx_buf, buf, len);
  s_tx_len  = len;
  s_tx_busy = 1;
  return 0;
}

void usb_cli_on_activate(void *cdc_acm_instance)
{
  s_cdc = (UX_SLAVE_CLASS_CDC_ACM *)cdc_acm_instance;
}

void usb_cli_on_deactivate(void)
{
  s_cdc     = UX_NULL;
  s_tx_busy = 0;
  s_tx_len  = 0;
}

void usb_cli_init(void)
{
  cli_init(usb_cli_tx);
}

void usb_cli_process(void)
{
  /* Drive the USBX device stack (enumeration, transfers) - required in
     standalone mode and independent of whether CDC is configured. */
  ux_system_tasks_run();

  /* Let the CLI process input and flush output (may call usb_cli_tx). */
  cli_process();

  if (s_cdc == UX_NULL)
  {
    return;
  }

  /* Transmit: run the write state machine until the staged chunk is sent. */
  if (s_tx_busy)
  {
    ULONG actual = 0;
    UINT  st = ux_device_class_cdc_acm_write_run(s_cdc, s_tx_buf, s_tx_len, &actual);
    if (st != UX_STATE_WAIT)
    {
      /* Done, error or aborted: release the staging buffer. */
      s_tx_busy = 0;
      s_tx_len  = 0;
    }
  }

  /* Receive: run the read state machine; feed completed packets to the CLI. */
  {
    ULONG actual = 0;
    UINT  st = ux_device_class_cdc_acm_read_run(s_cdc, s_rx, sizeof(s_rx), &actual);
    if (st == UX_STATE_NEXT && actual > 0)
    {
      cli_feed(s_rx, actual);
    }
  }
}

/**
 ******************************************************************************
 * @file    usb_cli.c
 * @brief   CLI over USBX CDC ACM (standalone). Bridges cli.c to USBX.
 ******************************************************************************
 */
#include "usb_cli.h"
#include "cli.h"
#include "usb.h"   /* hpcd_USB_DRD_FS, HAL PCD/PCDEx */

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

void usb_cli_start(void)
{
  /* USB_DRD_FS packet memory (PMA): allocate a buffer for every endpoint. USBX's
     STM32 DCD opens endpoints via HAL_PCD_EP_Open but never configures the PMA,
     so without this EP0 control transfers move garbage and the host reports an
     invalid / 0-byte device descriptor. Addresses are offsets into the 2 KB USB
     SRAM (BTABLE kept below 0x40); spacing respects each endpoint's max packet. */
  HAL_PCDEx_PMAConfig(&hpcd_USB_DRD_FS, 0x00U, PCD_SNG_BUF, 0x40);  /* EP0 OUT,      MPS 64 */
  HAL_PCDEx_PMAConfig(&hpcd_USB_DRD_FS, 0x80U, PCD_SNG_BUF, 0x80);  /* EP0 IN,       MPS 64 */
  HAL_PCDEx_PMAConfig(&hpcd_USB_DRD_FS, 0x81U, PCD_SNG_BUF, 0xC0);  /* CDC notify IN, MPS 8 */
  HAL_PCDEx_PMAConfig(&hpcd_USB_DRD_FS, 0x82U, PCD_SNG_BUF, 0xD0);  /* CDC bulk IN,  MPS 64 */
  HAL_PCDEx_PMAConfig(&hpcd_USB_DRD_FS, 0x03U, PCD_SNG_BUF, 0x110); /* CDC bulk OUT, MPS 64 */

  /* Connect the device to the bus (assert DP pull-up). MX_USBX_Init set up the
     stack/DCD but does not start it; without this the host sees no device.
     USBX (standalone) is then serviced from the loop via usb_cli_process(). */
  HAL_PCD_Start(&hpcd_USB_DRD_FS);

  /* Wire the CLI to the USB CDC transport. */
  cli_init(usb_cli_tx);
}

/* Per-transfer host stall timeout for the blocking write path. If the host
   stops draining the bulk-IN endpoint, give up instead of hanging the board. */
#define USB_TX_TIMEOUT_MS 2000u

bool usb_cli_connected(void)
{
  return s_cdc != UX_NULL;
}

void usb_cli_pump(void)
{
  ux_system_tasks_run();
}

/* Finish any chunk already staged by usb_cli_tx() so the blocking path does not
   interleave with a console write that is still in flight. */
static void finish_staged_tx(void)
{
  uint32_t t0 = HAL_GetTick();
  while (s_tx_busy && s_cdc != UX_NULL)
  {
    ULONG actual = 0;
    ux_system_tasks_run();
    UINT st = ux_device_class_cdc_acm_write_run(s_cdc, s_tx_buf, s_tx_len, &actual);
    if (st != UX_STATE_WAIT)
    {
      s_tx_busy = 0;
      s_tx_len  = 0;
      break;
    }
    if ((HAL_GetTick() - t0) >= USB_TX_TIMEOUT_MS)
    {
      break;
    }
  }
}

int usb_cli_write_blocking(const uint8_t *data, uint32_t len)
{
  if (s_cdc == UX_NULL || data == NULL)
  {
    return 1;
  }
  finish_staged_tx();

  uint32_t off = 0;
  while (off < len)
  {
    ULONG    actual = 0;
    UINT     st;
    uint32_t t0 = HAL_GetTick();
    /* Drive one write transaction for the remaining span to completion. USBX
       splits it into bulk packets internally and returns UX_STATE_NEXT once the
       whole requested length has been accepted. */
    do
    {
      ux_system_tasks_run();
      st = ux_device_class_cdc_acm_write_run(s_cdc, (UCHAR *)(data + off),
                                             (ULONG)(len - off), &actual);
      if (s_cdc == UX_NULL)
      {
        return 1; /* disconnected mid-transfer */
      }
    } while (st == UX_STATE_WAIT && (HAL_GetTick() - t0) < USB_TX_TIMEOUT_MS);

    if (st != UX_STATE_NEXT || actual == 0)
    {
      return 1; /* timeout, error, or no progress */
    }
    off += (uint32_t)actual;
  }
  return 0;
}

void usb_cli_flush_tx(void)
{
  finish_staged_tx();

  uint8_t tmp[64];
  size_t  n;
  while ((n = cli_take_tx(tmp, sizeof(tmp))) > 0u)
  {
    if (usb_cli_write_blocking(tmp, (uint32_t)n) != 0)
    {
      break;
    }
  }
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

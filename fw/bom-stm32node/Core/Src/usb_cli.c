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

/* Non-blocking stream send (pcm_stream ping-pong): one block is transmitted
   while the next is acquired. Buffer must stay valid until the send completes. */
static const uint8_t *s_bw_buf;
static uint32_t       s_bw_len;
static uint8_t        s_bw_active;

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
  s_cdc       = UX_NULL;
  s_tx_busy   = 0;
  s_tx_len    = 0;
  s_bw_buf    = UX_NULL;
  s_bw_len    = 0;
  s_bw_active = 0;
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
static bool finish_staged_tx(void)
{
  uint32_t t0 = HAL_GetTick();
  while (s_tx_busy && s_cdc != UX_NULL)
  {
    ULONG actual = 0;
    ux_system_tasks_run();
    UINT st = ux_device_class_cdc_acm_write_run(s_cdc, s_tx_buf, s_tx_len, &actual);
    if (st != UX_STATE_WAIT)
    {
      bool ok = st == UX_STATE_NEXT;
      s_tx_busy = 0;
      s_tx_len  = 0;
      if (!ok)
      {
        usb_cli_write_abort();
      }
      return ok;
    }
    if ((HAL_GetTick() - t0) >= USB_TX_TIMEOUT_MS)
    {
      usb_cli_write_abort();
      return false;
    }
  }
  return s_tx_busy == 0;
}

int usb_cli_write_blocking(const uint8_t *data, uint32_t len)
{
  if (s_cdc == UX_NULL || data == NULL)
  {
    return 1;
  }
  if (!finish_staged_tx())
  {
    return 1;
  }

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
      usb_cli_write_abort();
      return 1; /* timeout, error, or no progress */
    }
    off += (uint32_t)actual;
  }
  return 0;
}

int usb_cli_write_start(const uint8_t *data, uint32_t len)
{
  if (s_cdc == UX_NULL || data == NULL || s_bw_active)
  {
    return 1;
  }
  if (!finish_staged_tx()) /* don't collide with console output */
  {
    return 1;
  }
  s_bw_buf    = data;
  s_bw_len    = len;
  s_bw_active = 1;
  return 0;
}

int usb_cli_write_service(void)
{
  if (!s_bw_active)
  {
    return 0;
  }
  if (s_cdc == UX_NULL)
  {
    s_bw_active = 0;
    return -1;
  }
  ULONG actual = 0;
  ux_system_tasks_run();
  /* write_run tracks its own progress; it returns UX_STATE_NEXT once the whole
     length is accepted (call it with the original buffer/length each time). */
  UINT st = ux_device_class_cdc_acm_write_run(s_cdc, (UCHAR *)s_bw_buf,
                                              (ULONG)s_bw_len, &actual);
  if (st == UX_STATE_WAIT)
  {
    return 1; /* still sending */
  }
  s_bw_active = 0;
  if (st == UX_STATE_NEXT)
  {
    return 0;
  }
  usb_cli_write_abort();
  return -1;
}

bool usb_cli_write_active(void)
{
  return s_bw_active != 0;
}

void usb_cli_write_abort(void)
{
  /* All callers share one CDC IN state machine. Drop the application-side
     bookkeeping together so no later writer mistakes an aborted console or
     binary transaction for its own completion. */
  s_tx_busy = 0;
  s_tx_len  = 0;
  s_bw_active = 0;
  s_bw_buf    = UX_NULL;
  s_bw_len    = 0;
  if (s_cdc != UX_NULL)
  {
    /* Abort the bulk-IN pipe. This cancels any transfer still armed on the
       controller AND resets the class write state machine to UX_STATE_RESET,
       so the next usb_cli_write_blocking()/usb_cli_write_start() starts clean
       instead of re-entering a half-finished transfer with a stale buffer. */
    (void)ux_device_class_cdc_acm_ioctl(s_cdc,
                                        UX_SLAVE_CLASS_CDC_ACM_IOCTL_ABORT_PIPE,
                                        (VOID *)UX_SLAVE_CLASS_CDC_ACM_ENDPOINT_XMIT);
  }
}

bool usb_cli_flush_tx(void)
{
  if (!finish_staged_tx())
  {
    return false;
  }

  uint8_t tmp[64];
  size_t  n;
  while ((n = cli_take_tx(tmp, sizeof(tmp))) > 0u)
  {
    if (usb_cli_write_blocking(tmp, (uint32_t)n) != 0)
    {
      return false;
    }
  }
  return true;
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

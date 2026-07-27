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

/* CDC ACM class instance. The USB reset/disconnect callback (HAL_PCD_ResetCallback,
   USB IRQ, priority 0) preempts everything and runs usb_cli_on_deactivate, so this
   is the one static the ISR writes.
   INVARIANT: the ISR only ever writes s_cdc and s_pipe_reset_pending; every other
   static below is main-loop-only (hence non-volatile). Every consumer must snapshot
   s_cdc into a local once and guard the local - never re-read s_cdc between the
   NULL-check and passing it to a USBX call. The instance lives in the static USBX
   byte pool and is never freed on disconnect, so a stale-but-non-NULL snapshot is
   safe: write_run/read_run bail on device_state != CONFIGURED. If you ever add an
   ISR write to another static here, it must become volatile too. */
static UX_SLAVE_CLASS_CDC_ACM * volatile s_cdc;

/* Set on (re)activate, consumed once by usb_cli_process to reset the CDC class
   state machines and app flags after a connect (see usb_cli_process). */
static volatile uint8_t s_pipe_reset_pending;

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
  /* Store the pointer first, then arm the reset so the main loop that observes a
     non-NULL s_cdc also sees the pending flag. */
  s_cdc = (UX_SLAVE_CLASS_CDC_ACM *)cdc_acm_instance;
  s_pipe_reset_pending = 1;
}

void usb_cli_on_deactivate(void)
{
  /* Runs in USB IRQ context: do the minimum - drop the connection pointer with a
     single atomic aligned write. The app flags and the CDC class state machines
     are reset by the main loop on the next (re)connect (usb_cli_process). */
  s_cdc = UX_NULL;
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
  HAL_PCDEx_PMAConfig(&hpcd_USB_DRD_FS, 0x03U, PCD_SNG_BUF, 0x110); /* CDC bulk OUT, MPS 64 */
  /* CDC bulk IN: single-buffered. It was double-buffered as a workaround while
     the core was accidentally running at ~83 MHz (HSE was the 8 MHz ST-Link MCO,
     not the assumed 24 MHz): the DSP could not feed the endpoint fast enough and
     the single buffer NAK-throttled the stream. With the clock corrected to
     250 MHz the CPU has ample TX slack (~4 ms per 21 ms ring half), so a single
     buffer sustains full-speed bulk with overrun=0. Double buffering also left
     the endpoint's buffer-toggle desynced across streams (packet parity differs
     per stream), which hung the pipe after the 2nd stream (trailer lost, then
     the whole CDC stalled) - single buffering removes that failure mode. */
  HAL_PCDEx_PMAConfig(&hpcd_USB_DRD_FS, 0x82U, PCD_SNG_BUF, 0xD0);

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
  UX_SLAVE_CLASS_CDC_ACM *cdc = s_cdc;
  if (cdc == UX_NULL)
  {
    return s_tx_busy == 0;
  }
  uint32_t t0 = HAL_GetTick();
  while (s_tx_busy)
  {
    ULONG actual = 0;
    ux_system_tasks_run();
    /* On disconnect write_run returns non-WAIT (device left CONFIGURED); we treat
       that as failure below, so the stale-but-valid cdc snapshot is safe. */
    UINT st = ux_device_class_cdc_acm_write_run(cdc, s_tx_buf, s_tx_len, &actual);
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
  UX_SLAVE_CLASS_CDC_ACM *cdc = s_cdc;
  if (cdc == UX_NULL || data == NULL)
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
       whole requested length has been accepted. On disconnect write_run leaves
       UX_STATE_WAIT (returns non-NEXT), so the loop below aborts and returns 1. */
    do
    {
      ux_system_tasks_run();
      st = ux_device_class_cdc_acm_write_run(cdc, (UCHAR *)(data + off),
                                             (ULONG)(len - off), &actual);
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
  UX_SLAVE_CLASS_CDC_ACM *cdc = s_cdc;
  if (cdc == UX_NULL)
  {
    s_bw_active = 0;
    return -1;
  }
  ULONG actual = 0;
  ux_system_tasks_run();
  /* write_run tracks its own progress; it returns UX_STATE_NEXT once the whole
     length is accepted (call it with the original buffer/length each time). */
  UINT st = ux_device_class_cdc_acm_write_run(cdc, (UCHAR *)s_bw_buf,
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
  UX_SLAVE_CLASS_CDC_ACM *cdc = s_cdc;
  s_tx_busy = 0;
  s_tx_len  = 0;
  s_bw_active = 0;
  s_bw_buf    = UX_NULL;
  s_bw_len    = 0;
  if (cdc != UX_NULL)
  {
    /* Abort the bulk-IN pipe. This cancels any transfer still armed on the
       controller AND resets the class write state machine to UX_STATE_RESET,
       so the next usb_cli_write_blocking()/usb_cli_write_start() starts clean
       instead of re-entering a half-finished transfer with a stale buffer. */
    (void)ux_device_class_cdc_acm_ioctl(cdc,
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

  UX_SLAVE_CLASS_CDC_ACM *cdc = s_cdc; /* single snapshot for this iteration */
  if (cdc == UX_NULL)
  {
    return;
  }

  /* First service after a (re)connect: reset stale app bookkeeping and the CDC
     class write/read state machines. On unplug the deactivate ISR only nulls
     s_cdc; without this, a stale write_state/read_state would resume a
     half-finished transfer with a stale buffer on the first transfer after
     replug (garbage). Runs before any write_run/read_run below, and before any
     stream command (dispatched inside cli_process on a later iteration). */
  if (s_pipe_reset_pending)
  {
    s_pipe_reset_pending = 0;
    s_tx_busy   = 0;
    s_tx_len    = 0;
    s_bw_active = 0;
    s_bw_buf    = UX_NULL;
    s_bw_len    = 0;
    (void)ux_device_class_cdc_acm_ioctl(cdc, UX_SLAVE_CLASS_CDC_ACM_IOCTL_ABORT_PIPE,
                                        (VOID *)UX_SLAVE_CLASS_CDC_ACM_ENDPOINT_XMIT);
    (void)ux_device_class_cdc_acm_ioctl(cdc, UX_SLAVE_CLASS_CDC_ACM_IOCTL_ABORT_PIPE,
                                        (VOID *)UX_SLAVE_CLASS_CDC_ACM_ENDPOINT_RCV);
  }

  /* Transmit: run the write state machine until the staged chunk is sent. */
  if (s_tx_busy)
  {
    ULONG actual = 0;
    UINT  st = ux_device_class_cdc_acm_write_run(cdc, s_tx_buf, s_tx_len, &actual);
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
    UINT  st = ux_device_class_cdc_acm_read_run(cdc, s_rx, sizeof(s_rx), &actual);
    if (st == UX_STATE_NEXT && actual > 0)
    {
      cli_feed(s_rx, actual);
    }
  }
}

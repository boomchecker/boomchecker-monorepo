/**
 ******************************************************************************
 * @file    cli.c
 * @brief   Console on top of embedded-cli with output via a transport callback.
 ******************************************************************************
 */
#include "cli.h"
#include "embedded_cli.h"
#include "main.h"   /* Error_Handler */
#include "pcm_stream.h"
#include "detector.h"
#include "dfu_boot.h"
#include "usb_cli.h" /* flush before the DFU jump */

#include <stdlib.h> /* strtoul */
#include <string.h> /* memcpy  */

/* Static CLI allocation (no malloc). Sized for the rx/cmd/history below plus bindings. */
#define CLI_STATIC_BYTES  2048u
#define CLI_TX_RING       512u

static EmbeddedCli *s_cli;
static CLI_UINT     s_cli_buffer[BYTES_TO_CLI_UINTS(CLI_STATIC_BYTES)];
static cli_tx_fn    s_tx;

/* TX ring: writeChar fills it byte by byte, cli_process flushes in chunks. */
static uint8_t  s_tx_ring[CLI_TX_RING];
static uint16_t s_tx_head; /* write */
static uint16_t s_tx_tail; /* read */

static void tx_ring_push(uint8_t b)
{
  uint16_t next = (uint16_t)((s_tx_head + 1u) % CLI_TX_RING);
  if (next != s_tx_tail)
  {
    s_tx_ring[s_tx_head] = b;
    s_tx_head = next;
  }
  /* otherwise full -> drop the byte (prefer losing output over blocking) */
}

static void cli_write_char(EmbeddedCli *cli, char c)
{
  (void)cli;
  tx_ring_push((uint8_t)c);
}

/* Send one contiguous span of the ring (up to its end). If the transport is
   busy, leave the data in the ring and retry on the next cli_process. */
static void tx_flush(void)
{
  if (s_tx == NULL || s_tx_head == s_tx_tail)
  {
    return;
  }
  uint16_t tail = s_tx_tail;
  uint16_t len  = (s_tx_head > tail) ? (uint16_t)(s_tx_head - tail)
                                     : (uint16_t)(CLI_TX_RING - tail);
  if (s_tx(&s_tx_ring[tail], len) == 0)
  {
    s_tx_tail = (uint16_t)((tail + len) % CLI_TX_RING);
  }
}

/* --- Commands (help is built into embedded-cli) ---------------------------- */
static void cmd_version(EmbeddedCli *cli, char *args, void *context)
{
  (void)args;
  (void)context;
  embeddedCliPrint(cli, "bom-stm32node CLI v0.1");
}

/* Parse "<sec>" and run a PCM stream from the given source. Shared by the
   `stream` (microphone) and `streamtest` (synthetic tone) commands. */
static void stream_command(EmbeddedCli *cli, char *args, pcm_src_t src)
{
  if (embeddedCliGetTokenCount(args) < 1)
  {
    embeddedCliPrint(cli, "usage: stream <sec> (1..60)");
    return;
  }
  const char *tok = embeddedCliGetToken(args, 1);
  char        *end = NULL;
  unsigned long sec = strtoul(tok, &end, 10);
  if (end == tok || sec == 0u || sec > PCM_STREAM_MAX_SECONDS)
  {
    embeddedCliPrint(cli, "usage: stream <sec> (1..60)");
    return;
  }
  /* Emits a binary PCM1 frame on the same CDC pipe; see pcm_stream.c. */
  pcm_stream_run((uint32_t)sec, src);
}

static void cmd_stream(EmbeddedCli *cli, char *args, void *context)
{
  (void)context;
  stream_command(cli, args, PCM_SRC_MIC);
}

static void cmd_streamtest(EmbeddedCli *cli, char *args, void *context)
{
  (void)context;
  stream_command(cli, args, PCM_SRC_TONE);
}

static void cmd_detect(EmbeddedCli *cli, char *args, void *context)
{
  (void)context;
  const uint16_t ntok = embeddedCliGetTokenCount(args);
  if (ntok < 1 || ntok > 4)
  {
    embeddedCliPrint(cli, "usage: detect <sec> [squelch_milli] [thr_milli] [dbg]");
    return;
  }
  const char   *tok = embeddedCliGetToken(args, 1);
  char         *end = NULL;
  unsigned long sec = strtoul(tok, &end, 10);
  if (end == tok || sec == 0u || sec > 60u)
  {
    embeddedCliPrint(cli, "usage: detect <sec> (1..60)");
    return;
  }

  unsigned long squelch = DETECTOR_DEFAULT_SQUELCH_MILLI;
  long          thr     = DETECTOR_DEFAULT_THR_MILLI;
  if (ntok >= 2)
  {
    tok     = embeddedCliGetToken(args, 2);
    squelch = strtoul(tok, &end, 10);
    if (end == tok || squelch > 1000u)
    {
      embeddedCliPrint(cli, "squelch_milli: 0..1000 (10 = RMS 0.010)");
      return;
    }
  }
  if (ntok >= 3)
  {
    tok = embeddedCliGetToken(args, 3);
    thr = strtol(tok, &end, 10);
    if (end == tok || thr < -20000 || thr > 20000)
    {
      embeddedCliPrint(cli, "thr_milli: -20000..20000 (7250 = logit 7.25)");
      return;
    }
  }
  unsigned long dbg = 0u;
  if (ntok >= 4)
  {
    tok = embeddedCliGetToken(args, 4);
    dbg = strtoul(tok, &end, 10);
  }
  /* Emits LVL/DET/DETEND text lines on the console; see detector.c. */
  detector_run((uint32_t)sec, (uint32_t)squelch, (int32_t)thr, (uint32_t)dbg);
}

static void cmd_dfu(EmbeddedCli *cli, char *args, void *context)
{
  (void)cli;
  (void)args;
  (void)context;
  /* Bypass the CLI TX ring: the jump never returns, so push the farewell out
     synchronously before detaching from the bus. */
  static const char msg[] = "DFU: rebooting into the ROM bootloader\n";
  (void)usb_cli_flush_tx();
  (void)usb_cli_write_blocking((const uint8_t *)msg, sizeof(msg) - 1u);
  dfu_boot_enter();
}

void cli_init(cli_tx_fn tx)
{
  s_tx      = tx;
  s_tx_head = 0;
  s_tx_tail = 0;

  EmbeddedCliConfig *cfg = embeddedCliDefaultConfig();
  cfg->cliBuffer         = s_cli_buffer;
  cfg->cliBufferSize     = CLI_STATIC_BYTES;
  cfg->rxBufferSize      = 64;
  cfg->cmdBufferSize     = 64;
  cfg->historyBufferSize = 128;
  cfg->maxBindingCount   = 8;
  cfg->invitation        = "> ";

  s_cli = embeddedCliNew(cfg);
  if (s_cli == NULL)
  {
    /* Static buffer too small. Do NOT trap here: the USB device stack must keep
       being serviced from the main loop, so a CLI failure must not dead-loop.
       cli_process()/cli_feed() are NULL-guarded and simply no-op. */
    return;
  }
  s_cli->writeChar = cli_write_char;

  CliCommandBinding version_binding = {
    .name         = "version",
    .help         = "Print firmware version",
    .tokenizeArgs = false,
    .context      = NULL,
    .binding      = cmd_version,
  };
  embeddedCliAddBinding(s_cli, version_binding);

  CliCommandBinding stream_binding = {
    .name         = "stream",
    .help         = "Stream <sec> seconds of microphone PCM (binary PCM1 frame)",
    .tokenizeArgs = true,
    .context      = NULL,
    .binding      = cmd_stream,
  };
  embeddedCliAddBinding(s_cli, stream_binding);

  CliCommandBinding streamtest_binding = {
    .name         = "streamtest",
    .help         = "Stream <sec> seconds of a synthetic 1 kHz test tone",
    .tokenizeArgs = true,
    .context      = NULL,
    .binding      = cmd_streamtest,
  };
  embeddedCliAddBinding(s_cli, streamtest_binding);

  CliCommandBinding detect_binding = {
    .name         = "detect",
    .help         = "Run drone detection for <sec> seconds (DET/DETEND lines)",
    .tokenizeArgs = true,
    .context      = NULL,
    .binding      = cmd_detect,
  };
  embeddedCliAddBinding(s_cli, detect_binding);

  CliCommandBinding dfu_binding = {
    .name         = "dfu",
    .help         = "Reboot into the ROM bootloader for USB DFU flashing",
    .tokenizeArgs = false,
    .context      = NULL,
    .binding      = cmd_dfu,
  };
  embeddedCliAddBinding(s_cli, dfu_binding);

  embeddedCliProcess(s_cli); /* print the initial prompt */
}

void cli_process(void)
{
  if (s_cli == NULL)
  {
    return;
  }
  embeddedCliProcess(s_cli);
  tx_flush();
}

void cli_feed(const uint8_t *buf, uint32_t len)
{
  if (s_cli == NULL || buf == NULL)
  {
    return;
  }
  for (uint32_t i = 0; i < len; i++)
  {
    embeddedCliReceiveChar(s_cli, (char)buf[i]);
  }
}

size_t cli_take_tx(uint8_t *dst, size_t max)
{
  if (dst == NULL || max == 0u || s_tx_head == s_tx_tail)
  {
    return 0;
  }
  uint16_t tail = s_tx_tail;
  uint16_t span = (s_tx_head > tail) ? (uint16_t)(s_tx_head - tail)
                                     : (uint16_t)(CLI_TX_RING - tail);
  if (span > max)
  {
    span = (uint16_t)max;
  }
  memcpy(dst, &s_tx_ring[tail], span);
  s_tx_tail = (uint16_t)((tail + span) % CLI_TX_RING);
  return span;
}

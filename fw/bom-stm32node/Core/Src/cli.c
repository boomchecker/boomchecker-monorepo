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
#include "radio.h"

#include <stdio.h>  /* snprintf */
#include <stdlib.h> /* strtoul */
#include <string.h> /* memcpy, strcmp, strlen */

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

/* Render `value` with `decimals` digits after the point (max 3) using only
   integer arithmetic: this firmware links newlib-nano without _printf_float,
   so snprintf("%f", ...) is not available. */
static void fmt_fixed(char *buf, size_t buflen, float value, unsigned decimals)
{
  static const long pow10[4] = {1, 10, 100, 1000};
  /* Clamp once and reuse for both the lookup and the printed field width -
     the two must stay in lockstep, or a >3 caller would still print a wider
     zero-padded field than `scale` actually supports. */
  decimals = (decimals > 3u) ? 3u : decimals;
  long  scale  = pow10[decimals];
  bool  neg    = value < 0.0f;
  float mag    = neg ? -value : value;
  long  scaled = (long)(mag * (float)scale + 0.5f);
  long  whole  = scaled / scale;
  long  frac   = scaled % scale;
  snprintf(buf, buflen, "%s%ld.%0*ld", neg ? "-" : "", whole, (int)decimals, frac);
}

static void print_radio_status(EmbeddedCli *cli)
{
  /* Worst case ~124 bytes for the stats line below (two %lu counters at
     10 digits each plus two 15-char fmt_fixed() fields) - sized generously
     past that. */
  char line[160];
  char f1[16];
  char f2[16];

  if (!radio_is_ready())
  {
    snprintf(line, sizeof(line), "radio: not ready (error %d)", radio_last_error());
    embeddedCliPrint(cli, line);
    return;
  }

  radio_profile_t profile;
  radio_get_profile(&profile);
  fmt_fixed(f1, sizeof(f1), profile.frequency_mhz, 3);
  fmt_fixed(f2, sizeof(f2), profile.bandwidth_khz, 1);
  snprintf(line, sizeof(line),
           "radio: ready  %s MHz  BW %s kHz  SF%u CR4/%u  %d dBm  sync 0x%02X",
           f1, f2, (unsigned)profile.spreading_factor, (unsigned)profile.coding_rate_denom,
           (int)profile.tx_power_dbm, (unsigned)profile.sync_word);
  embeddedCliPrint(cli, line);

  radio_stats_t stats;
  radio_get_stats(&stats);
  fmt_fixed(f1, sizeof(f1), stats.last_rssi_dbm, 1);
  fmt_fixed(f2, sizeof(f2), stats.last_snr_db, 1);
  snprintf(line, sizeof(line),
           "  tx %lu (err %lu)  rx %lu (crc err %lu, overrun %lu)  last RSSI %s dBm  SNR %s dB",
           (unsigned long)stats.tx_packets, (unsigned long)stats.tx_errors,
           (unsigned long)stats.rx_packets, (unsigned long)stats.rx_crc_errors,
           (unsigned long)stats.rx_overruns, f1, f2);
  embeddedCliPrint(cli, line);

  /* A past send failure (radio_send() latches its RadioLib status code into
     the same radio_last_error() radio_is_ready() reads) would otherwise be
     invisible here once the radio is healthy again - only the not-ready
     branch above used to show it. */
  int last_error = radio_last_error();
  if (last_error != 0)
  {
    snprintf(line, sizeof(line), "  last error: %d", last_error);
    embeddedCliPrint(cli, line);
  }
}

/* Raw, unaddressed bring-up test: send `text` (default "PING") as one LoRa
   packet. Run it on one board while the other's console is watched for the
   "radio rx: ..." line that cli_process() prints automatically (see
   print_rx_frame()) - manually repeating this in both directions is the
   "raw RadioLib ping/pong" hardware test from boomlink.md section 15.3. */
static void cmd_radio(EmbeddedCli *cli, char *args, void *context)
{
  (void)context;
  const char *sub = embeddedCliGetToken(args, 1);

  if (sub != NULL && strcmp(sub, "status") == 0)
  {
    print_radio_status(cli);
    return;
  }

  if (sub != NULL && strcmp(sub, "ping") == 0)
  {
    const char *payload = embeddedCliGetToken(args, 2);
    if (payload == NULL)
    {
      payload = "PING";
    }
    size_t len = strlen(payload);
    int    rc  = radio_send((const uint8_t *)payload, len);

    /* Worst case ~83 bytes: fixed text plus a token up to the CLI's own
       ~52-char remaining command-buffer budget - sized generously past that. */
    char line[96];
    if (rc == 0)
    {
      snprintf(line, sizeof(line), "radio ping: sent \"%s\" (%u bytes)", payload, (unsigned)len);
    }
    else
    {
      snprintf(line, sizeof(line), "radio ping: failed (error %d)", rc);
    }
    embeddedCliPrint(cli, line);
    return;
  }

  if (sub != NULL && strcmp(sub, "reset") == 0)
  {
    radio_reset_stats();
    embeddedCliPrint(cli, "radio: stats reset");
    return;
  }

  embeddedCliPrint(cli, "usage: radio status | radio ping [text] | radio reset");
}

/* Longest RX payload previewed in the CLI; longer packets are still fully
   received/counted, just truncated for the debug print. */
#define RADIO_RX_PREVIEW_MAX 64u

/* Auto-print any packet radio_process() has finished receiving since the
   last call. Called every cli_process() tick so `radio ping` on another
   board shows up without a dedicated poll command. */
static void print_rx_frame(void)
{
  if (s_cli == NULL)
  {
    return;
  }

  uint8_t buf[RADIO_RX_PREVIEW_MAX];
  size_t  len  = 0;
  float   rssi = 0.0f;
  float   snr  = 0.0f;
  if (!radio_poll_rx(buf, sizeof(buf), &len, &rssi, &snr))
  {
    return;
  }

  /* Best-effort debug view of a raw bring-up payload (no BoomProtocol yet):
     unprintable bytes show as '.'. */
  char preview[RADIO_RX_PREVIEW_MAX + 1];
  size_t n = (len < sizeof(buf)) ? len : sizeof(buf);
  for (size_t i = 0; i < n; i++)
  {
    char c = (char)buf[i];
    preview[i] = (c >= 0x20 && c < 0x7f) ? c : '.';
  }
  preview[n] = '\0';

  char f1[16];
  char f2[16];
  fmt_fixed(f1, sizeof(f1), rssi, 1);
  fmt_fixed(f2, sizeof(f2), snr, 1);

  char line[RADIO_RX_PREVIEW_MAX + 96];
  snprintf(line, sizeof(line), "radio rx: \"%s\"%s (%u bytes, RSSI %s dBm, SNR %s dB)",
           preview, (len > sizeof(buf)) ? "..." : "", (unsigned)len, f1, f2);
  embeddedCliPrint(s_cli, line);
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

  CliCommandBinding radio_binding = {
    .name         = "radio",
    .help         = "radio status | radio ping [text] | radio reset - raw SX1262 bring-up test",
    .tokenizeArgs = true,
    .context      = NULL,
    .binding      = cmd_radio,
  };
  embeddedCliAddBinding(s_cli, radio_binding);

  embeddedCliProcess(s_cli); /* print the initial prompt */
}

void cli_process(void)
{
  if (s_cli == NULL)
  {
    return;
  }
  embeddedCliProcess(s_cli);
  print_rx_frame();
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

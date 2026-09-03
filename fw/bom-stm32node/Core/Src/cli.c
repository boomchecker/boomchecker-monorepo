/**
 ******************************************************************************
 * @file    cli.c
 * @brief   Console on top of embedded-cli with output via a transport callback.
 ******************************************************************************
 */
#include "cli.h"
#include "boomlink_codec.h"
#include "boomlink_system_service.h" /* BOOMLINK_SYSTEM_WAKEUP_MAX_WINDOW_S */
#include "embedded_cli.h"
#include "link_service.h"
#include "main.h"   /* Error_Handler */
#include "pcm_stream.h"
#include "detector.h"
#include "gps.h"
#include "mic.h"     /* mic_diag_run */
#include "dfu_boot.h"
#include "usb_cli.h" /* flush before the DFU jump */
#include "protocol_service.h"
#include "radio.h"

#include <stdio.h>  /* snprintf */
#include <stdlib.h> /* strtoul */
#include <string.h> /* memcmp, memcpy, strcmp, strlen */

/* Live cross-check between two independently-vendored packages:
   boomlink_codec.c can only hardcode its own assumed copy of the SX126x
   hard packet limit (see its comment on BOOMLINK_RADIO_MAX_PAYLOAD) since
   fw/common/boomlink must not depend on this firmware's App/ layer
   (boomlink.md section 4). This file already includes both radio.h and
   boomlink_codec.h, so it is where a check against the REAL compiled
   RADIO_MAX_PAYLOAD can live - if a future change shrinks that constant
   without anyone remembering to also update boomlink_codec.c, this fails
   the build instead of the drift going unnoticed until a real Envelope no
   longer fits on the air.

   Written as an addition compared against RADIO_MAX_PAYLOAD, not a
   subtraction compared against boomlink_Envelope_size: RADIO_MAX_PAYLOAD
   and BOOMLINK_LINK_FRAME_HEADER_SIZE are both unsigned, so
   "RADIO_MAX_PAYLOAD - BOOMLINK_LINK_FRAME_HEADER_SIZE" would silently
   wrap to a huge value (and the assert would wrongly pass) if
   RADIO_MAX_PAYLOAD were ever smaller than the header size - exactly the
   kind of drift this check exists to catch. */
_Static_assert(RADIO_MAX_PAYLOAD >= BOOMLINK_LINK_FRAME_HEADER_SIZE + boomlink_Envelope_size,
               "BoomLink Envelope no longer fits the real RADIO_MAX_PAYLOAD budget");

/* Static CLI allocation (no malloc). Sized for the rx/cmd/history below plus bindings. */
#define CLI_STATIC_BYTES  3072u
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
  /* PROTOCOL_SERVICE_FW_VERSION_MAJOR/MINOR/PATCH (protocol_service.h) - the
     same build version a section 8.6 WakeupResponse reports of this node -
     rather than a separately-maintained literal here that could drift from
     it. */
  /* Worst case ~19 bytes of fixed text + up to 30 bytes for three %u fields
     at 10 digits each + NUL = 50 - sized generously past that, the same
     "hand-computed, not eyeballed" convention print_radio_status() already
     documents, since the three macros are plain uint32_t and nothing here
     assumes they stay single-digit forever. */
  char line[64];
  snprintf(line, sizeof(line), "bom-stm32node CLI v%u.%u.%u", PROTOCOL_SERVICE_FW_VERSION_MAJOR,
           PROTOCOL_SERVICE_FW_VERSION_MINOR, PROTOCOL_SERVICE_FW_VERSION_PATCH);
  embeddedCliPrint(cli, line);
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

/* Optional "[baud]" token shared by `gps` and `gpstx`. Returns 0 on error. */
static uint32_t parse_baud(EmbeddedCli *cli, const char *tok)
{
  char         *end  = NULL;
  unsigned long baud = strtoul(tok, &end, 10);
  if (end == tok || baud < 1200u || baud > 921600u)
  {
    embeddedCliPrint(cli, "baud: 1200..921600 (Teseo-LIV3R default 9600)");
    return 0u;
  }
  return (uint32_t)baud;
}

static void cmd_gps(EmbeddedCli *cli, char *args, void *context)
{
  (void)context;
  const uint16_t ntok = embeddedCliGetTokenCount(args);
  if (ntok < 1 || ntok > 2)
  {
    embeddedCliPrint(cli, "usage: gps <sec> [baud]");
    return;
  }
  const char   *tok = embeddedCliGetToken(args, 1);
  char         *end = NULL;
  unsigned long sec = strtoul(tok, &end, 10);
  if (end == tok || sec == 0u || sec > GPS_MAX_SECONDS)
  {
    embeddedCliPrint(cli, "usage: gps <sec> (1..300)");
    return;
  }
  uint32_t baud = GPS_DEFAULT_BAUD;
  if (ntok >= 2)
  {
    baud = parse_baud(cli, embeddedCliGetToken(args, 2));
    if (baud == 0u)
    {
      return;
    }
  }
  /* Emits raw NMEA lines and a GPSEND trailer on the console; see gps.c. */
  gps_run((uint32_t)sec, baud);
}

static void cmd_gpstx(EmbeddedCli *cli, char *args, void *context)
{
  (void)context;
  const uint16_t ntok = embeddedCliGetTokenCount(args);
  if (ntok < 1 || ntok > 2)
  {
    embeddedCliPrint(cli, "usage: gpstx <sentence> [baud]");
    return;
  }
  uint32_t baud = GPS_DEFAULT_BAUD;
  if (ntok >= 2)
  {
    baud = parse_baud(cli, embeddedCliGetToken(args, 2));
    if (baud == 0u)
    {
      return;
    }
  }
  if (gps_send(embeddedCliGetToken(args, 1), baud) != 0)
  {
    embeddedCliPrint(cli, "GPSERR tx failed");
    return;
  }
  embeddedCliPrint(cli, "GPSTX ok");
}

static void cmd_micdiag(EmbeddedCli *cli, char *args, void *context)
{
  (void)cli;
  (void)args;
  (void)context;
  /* Emits MICDIAG lines + MICDIAGEND trailer on the console; see mic.c. */
  mic_diag_run();
}

static void cmd_gpsrst(EmbeddedCli *cli, char *args, void *context)
{
  (void)args;
  (void)context;
  gps_reset_pulse();
  embeddedCliPrint(cli, "GPSRST done (SYS_RSTn pulsed 100 ms)");
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
   "raw RadioLib ping/pong" hardware test from boomlink.md section 15.3.
   `radio ping`'s SEND side works regardless of `link enable`/`link
   disable` (radio_send() has no exclusivity issue - see radio.h). Its RX
   preview needs the *receiving* board to have run `link disable` first:
   radio_poll_rx() is single-consumer, and by default the link engine owns
   it (see link_service.h) - a raw, non-BoomLink-framed payload sent to a
   board that has NOT disabled the link engine is consumed by BoomLink
   instead, rejected as malformed/bad-magic, and never reaches this preview
   at all. */
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

/* BoomProtocol bring-up smoke test: build a Ping-carrying Envelope, encode
   it, decode the bytes back, and check every field survived. Exists so the
   PR 2 acceptance criterion "STM32 build can encode/decode an Envelope
   using Nanopb" is something this firmware actually does at runtime, not
   just a library the linker happens to include - proto.c's boomlink_protocol
   symbols would otherwise never be referenced by anything in the image.
   No BoomLink/radio transport involved yet (that is PR 3); this never
   touches the radio. */
static void proto_selftest(EmbeddedCli *cli)
{
  boomlink_Envelope envelope = boomlink_Envelope_init_zero;
  boomlink_envelope_init(&envelope);
  envelope.header.request_id            = 1;
  envelope.which_payload                = boomlink_Envelope_system_tag;
  envelope.payload.system.which_message = boomlink_SystemMessage_ping_tag;
  const uint8_t kPayload[]              = {'h', 'i'};
  /* Guards kPayload against a shrunk nanopb/system.options bound. This
     firmware has no sanitizer runtime (unlike tests/codec_tool.c's identical
     check) - without this, a max_size shrink below sizeof(kPayload) would
     silently overflow ping.payload.bytes on every boot that runs this
     selftest, caught by nothing until something else corrupted nearby. */
  _Static_assert(sizeof(((boomlink_Ping *)0)->payload.bytes) >= sizeof(kPayload),
                 "proto selftest's fixed payload no longer fits the compiled Ping payload bound");
  memcpy(envelope.payload.system.message.ping.payload.bytes, kPayload, sizeof(kPayload));
  envelope.payload.system.message.ping.payload.size = sizeof(kPayload);

  uint8_t buf[boomlink_Envelope_size];
  size_t  len = 0;
  if (!boomlink_encode_envelope(&envelope, buf, sizeof(buf), &len))
  {
    embeddedCliPrint(cli, "proto selftest: encode failed");
    return;
  }

  boomlink_Envelope decoded = boomlink_Envelope_init_zero;
  if (!boomlink_decode_envelope(buf, len, &decoded))
  {
    embeddedCliPrint(cli, "proto selftest: decode failed");
    return;
  }

  bool round_trip_ok =
      decoded.header.protocol_version == BOOMLINK_PROTOCOL_VERSION &&
      decoded.header.request_id == 1 && decoded.which_payload == boomlink_Envelope_system_tag &&
      decoded.payload.system.which_message == boomlink_SystemMessage_ping_tag &&
      decoded.payload.system.message.ping.payload.size == sizeof(kPayload) &&
      memcmp(decoded.payload.system.message.ping.payload.bytes, kPayload, sizeof(kPayload)) == 0;

  /* RADIO_MAX_PAYLOAD - BOOMLINK_LINK_FRAME_HEADER_SIZE cannot underflow
     here: the _Static_assert above already guarantees
     RADIO_MAX_PAYLOAD >= BOOMLINK_LINK_FRAME_HEADER_SIZE (it requires the
     sum of that and boomlink_Envelope_size to fit), so any binary that
     compiled this file at all satisfies it. */
  char line[96];
  snprintf(line, sizeof(line), "proto selftest: %s (%u bytes encoded, %u byte budget)",
           round_trip_ok ? "ok" : "FAILED", (unsigned)len,
           (unsigned)(RADIO_MAX_PAYLOAD - BOOMLINK_LINK_FRAME_HEADER_SIZE));
  embeddedCliPrint(cli, line);
}

static void cmd_proto(EmbeddedCli *cli, char *args, void *context)
{
  (void)context;
  const char *sub = embeddedCliGetToken(args, 1);

  if (sub != NULL && strcmp(sub, "selftest") == 0)
  {
    proto_selftest(cli);
    return;
  }

  embeddedCliPrint(cli, "usage: proto selftest");
}

/* Longest RX payload previewed in the CLI; longer packets are still fully
   received/counted, just truncated for the debug print. */
#define RADIO_RX_PREVIEW_MAX 64u

/* Render an opaque received payload for a human console preview, picking the
   representation from the bytes themselves so a binary frame is not shown as
   noise:
     - every byte printable ASCII -> the text in double quotes, e.g. "hello"
       (a raw `radio ping`/`link ping` text payload);
     - anything else -> an uppercase hex dump in brackets, e.g.
       hex[12 08 96 01], because a binary protobuf Envelope - a section 8.6
       WakeupResponse, say - rendered as text is just a row of dots
       ("....r."...) that tells the operator nothing.
   Writes at most `cap - 1` chars plus a NUL and returns the number of
   payload BYTES represented (not chars written), so the caller can still
   append a "..." marker when the payload was longer than what fit. `cap == 0`
   renders nothing rather than underflowing the size math; an empty payload
   renders as "". Shared by the raw and BoomLink-level RX previews below -
   both need exactly this, on payloads from two different sources. */
static size_t payload_preview(char *out, size_t cap, const uint8_t *buf, size_t len)
{
  if (cap == 0u)
  {
    return 0;
  }
  out[0] = '\0';

  /* An all-printable payload (and the empty payload) is text; a single
     non-printable byte makes the whole thing binary and flips it to hex. */
  bool is_text = true;
  for (size_t i = 0; i < len; i++)
  {
    char c = (char)buf[i];
    if (!(c >= 0x20 && c < 0x7f))
    {
      is_text = false;
      break;
    }
  }

  size_t used = 0u;
  if (is_text)
  {
    /* Fixed overhead: opening quote + closing quote + NUL = 3 chars. */
    if (cap < 3u)
    {
      return 0;
    }
    out[used++] = '"';
    size_t i = 0u;
    while (i < len && used < cap - 2u)
    {
      out[used++] = (char)buf[i++];
    }
    out[used++] = '"';
    out[used]   = '\0';
    return i;
  }

  /* hex[...] : "hex[" (4) + "]" + NUL (2) fixed, then "XX " (3) per byte. */
  static const char kHexDigits[] = "0123456789ABCDEF";
  if (cap < 6u)
  {
    return 0;
  }
  out[used++] = 'h';
  out[used++] = 'e';
  out[used++] = 'x';
  out[used++] = '[';
  size_t i = 0u;
  while (i < len && used + 3u <= cap - 2u)
  {
    out[used++] = kHexDigits[(buf[i] >> 4) & 0x0Fu];
    out[used++] = kHexDigits[buf[i] & 0x0Fu];
    out[used++] = ' ';
    i++;
  }
  if (i > 0u && out[used - 1u] == ' ')
  {
    used--; /* trim the trailing separator before the closing bracket */
  }
  out[used++] = ']';
  out[used]   = '\0';
  return i;
}

/* Auto-print any packet radio_process() has finished receiving since the
   last call. Called every cli_process() tick so `radio ping` on another
   board shows up without a dedicated poll command - but only while the
   link engine does not own radio_poll_rx() (see link_service_enabled()):
   radio.h's poll is single-consumer, and by default the link engine is the
   one consumer. `link disable` hands it back to this. */
static void print_rx_frame(void)
{
  if (s_cli == NULL || link_service_enabled())
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

  /* Best-effort debug view of a raw bring-up payload (no BoomProtocol yet). */
  char   preview[RADIO_RX_PREVIEW_MAX + 1];
  size_t shown = payload_preview(preview, sizeof(preview), buf, len);

  char f1[16];
  char f2[16];
  fmt_fixed(f1, sizeof(f1), rssi, 1);
  fmt_fixed(f2, sizeof(f2), snr, 1);

  char line[RADIO_RX_PREVIEW_MAX + 96];
  snprintf(line, sizeof(line), "radio rx: %s%s (%u bytes, RSSI %s dBm, SNR %s dB)",
           preview, (len > shown) ? " ..." : "", (unsigned)len, f1, f2);
  embeddedCliPrint(s_cli, line);
}

static const char *tx_state_name(boomlink_tx_state_t state)
{
  switch (state)
  {
    case BOOMLINK_TX_STATE_IDLE:     return "idle";
    case BOOMLINK_TX_STATE_READY:    return "ready";
    case BOOMLINK_TX_STATE_JITTER:   return "jitter";
    case BOOMLINK_TX_STATE_WAIT_ACK: return "wait-ack";
    case BOOMLINK_TX_STATE_BACKOFF:  return "backoff";
    default:                         return "?";
  }
}

static const char *send_result_name(boomlink_link_send_result_t rc)
{
  switch (rc)
  {
    case BOOMLINK_LINK_SEND_OK:              return "queued";
    case BOOMLINK_LINK_SEND_OK_EVICTED:      return "queued (evicted lower-priority traffic)";
    case BOOMLINK_LINK_SEND_QUEUE_FULL:      return "failed: queue full";
    case BOOMLINK_LINK_SEND_TOO_LONG:        return "failed: too long for this radio";
    case BOOMLINK_LINK_SEND_BAD_DESTINATION: return "failed: bad destination";
    default:                                 return "failed: unknown";
  }
}

static const char *tx_outcome_name(boomlink_tx_outcome_t outcome)
{
  switch (outcome)
  {
    case BOOMLINK_TX_SENT:   return "sent (no ack requested)";
    case BOOMLINK_TX_ACKED:  return "acked";
    case BOOMLINK_TX_NO_ACK: return "no ack (final failure)";
    default:                return "?";
  }
}

/* boomlink_link_rx_fn - prints an accepted DATA frame's payload. The
   BoomLink-level counterpart to print_rx_frame()'s raw preview, with the
   addressing and per-packet signal quality only this layer knows. Called
   from link_service_process() (cli_process() -> boomlink_link_poll()), so
   never from interrupt context; `payload` is only valid for this call,
   which is exactly how long the preview needs it. */
static void link_on_rx(void *user, uint32_t source_id, uint32_t destination_id,
                       const uint8_t *payload, size_t payload_len, float rssi_dbm,
                       float snr_db)
{
  if (s_cli == NULL)
  {
    return;
  }

  char   preview[RADIO_RX_PREVIEW_MAX + 1];
  size_t shown = payload_preview(preview, sizeof(preview), payload, payload_len);

  char f1[16];
  char f2[16];
  fmt_fixed(f1, sizeof(f1), rssi_dbm, 1);
  fmt_fixed(f2, sizeof(f2), snr_db, 1);

  char line[RADIO_RX_PREVIEW_MAX + 128];
  snprintf(line, sizeof(line),
           "link rx: from 0x%08lX to %s %s%s (%u bytes, RSSI %s dBm, SNR %s dB)",
           (unsigned long)source_id,
           (destination_id == BOOMLINK_ADDR_BROADCAST) ? "broadcast" : "me", preview,
           (payload_len > shown) ? " ..." : "", (unsigned)payload_len, f1, f2);
  embeddedCliPrint(s_cli, line);

  /* PR 4 Phase C: the dispatcher wiring boomlink_link_rx_fn's own doc
     already names this exact call site for. This debug preview stays
     first/unconditional (it does not need a successful decode - a raw
     preview of garbage is still useful for bring-up), then the real
     BoomProtocol dispatch runs on the same bytes and is a silent no-op if
     they do not decode as an Envelope (see protocol_service_on_rx()'s own
     doc). */
  protocol_service_on_rx(user, source_id, destination_id, payload, payload_len, rssi_dbm, snr_db);
}

/* boomlink_link_tx_done_fn - prints the outcome of a `link ping`. Called
   with the TX pipeline already back to idle (see the typedef's own doc), so
   nothing here re-enters boomlink_link_poll(). rssi_dbm/snr_db are only a
   real reading for BOOMLINK_TX_ACKED (0.0f is the "nothing received"
   sentinel otherwise - see boomlink_link_tx_done_fn) so they are only
   printed then, rather than as a plausible-looking zero. */
static void link_on_tx_done(void *user, boomlink_tx_outcome_t outcome,
                            uint32_t destination_id, uint32_t sequence, uint8_t attempts,
                            float rssi_dbm, float snr_db)
{
  (void)user;
  if (s_cli == NULL)
  {
    return;
  }

  char line[128];
  if (outcome == BOOMLINK_TX_ACKED)
  {
    char f1[16];
    char f2[16];
    fmt_fixed(f1, sizeof(f1), rssi_dbm, 1);
    fmt_fixed(f2, sizeof(f2), snr_db, 1);
    snprintf(line, sizeof(line),
             "link tx done: to 0x%08lX seq %lu attempts %u - %s (RSSI %s dBm, SNR %s dB)",
             (unsigned long)destination_id, (unsigned long)sequence, (unsigned)attempts,
             tx_outcome_name(outcome), f1, f2);
  }
  else
  {
    snprintf(line, sizeof(line), "link tx done: to 0x%08lX seq %lu attempts %u - %s",
             (unsigned long)destination_id, (unsigned long)sequence, (unsigned)attempts,
             tx_outcome_name(outcome));
  }
  embeddedCliPrint(s_cli, line);
}

static void print_link_status(EmbeddedCli *cli)
{
  boomlink_link_t *link = link_service_link();
  if (link == NULL)
  {
    embeddedCliPrint(cli, "link: not initialized");
    return;
  }

  /* Worst case (the ack/rx-rejection counters line, nine %lu fields at up
     to 10 digits each) is 183 bytes + NUL = 184 - computed by hand, not
     estimated, since this line reuses the same buffer for three different
     format strings of different lengths. 224 leaves genuine margin past
     that, rather than the 8 bytes a smaller buffer would. */
  char line[224];
  snprintf(line, sizeof(line),
           "link: node 0x%08lX  session 0x%08lX  %s  queue %u  radio-poll: %s",
           (unsigned long)link_service_node_id(), (unsigned long)boomlink_link_session_id(link),
           tx_state_name(boomlink_link_tx_state(link)), (unsigned)boomlink_link_queue_depth(link),
           link_service_enabled() ? "link" : "raw (see 'link enable')");
  embeddedCliPrint(cli, line);

  boomlink_link_stats_t stats;
  boomlink_link_get_stats(link, &stats);

  char f1[16];
  char f2[16];
  fmt_fixed(f1, sizeof(f1), stats.last_rssi_dbm, 1);
  fmt_fixed(f2, sizeof(f2), stats.last_snr_db, 1);
  snprintf(line, sizeof(line),
           "  tx %lu (retry %lu, fail %lu)  rx %lu (dup %lu, malformed %lu)  last RSSI %s dBm  SNR %s dB",
           (unsigned long)stats.tx_envelopes, (unsigned long)stats.tx_retries,
           (unsigned long)stats.tx_failures, (unsigned long)stats.rx_envelopes,
           (unsigned long)stats.rx_duplicates, (unsigned long)stats.rx_malformed, f1, f2);
  embeddedCliPrint(cli, line);

  snprintf(line, sizeof(line),
           "  ack sent %lu recv %lu unmatched %lu  rx other-dest %lu bad-magic %lu bad-src %lu "
           "oversize %lu  tx dropped %lu shed %lu",
           (unsigned long)stats.ack_sent, (unsigned long)stats.ack_received,
           (unsigned long)stats.ack_unmatched, (unsigned long)stats.rx_other_destination,
           (unsigned long)stats.rx_rejected_magic_or_version,
           (unsigned long)stats.rx_invalid_source, (unsigned long)stats.rx_oversize,
           (unsigned long)stats.tx_dropped, (unsigned long)stats.tx_shed);
  embeddedCliPrint(cli, line);
}

/* Parses `tok` as an optional "0x"/"0X" prefix followed by EXACTLY 1-8 hex
   digits (no sign) into `*out`. Deliberately not strtoul(): it accepts a
   leading '+'/'-' (so
   "-1" silently parses as a huge unsigned value) and reports overflow by
   returning ULONG_MAX/setting errno rather than by refusing to parse, so a
   caller checking only "did the end pointer move" - the check this file
   used before this function existed - accepts "1FFFFFFFF" as
   0xFFFFFFFF... which happens to be BOOMLINK_ADDR_BROADCAST, turning a
   typo into a real, silent, un-acked broadcast transmission (section
   9.9's "broadcast never requests link ACK" only makes that quieter).
   Capping at 8 digits makes the accumulation below incapable of
   overflowing uint32_t in the first place - no ERANGE check needed.

   Accepts an optional leading "0x"/"0X", stripped before the 1-8-digit
   count below applies - not for strtoul() compatibility, but because
   `link status` PRINTS every ID this way ("node 0x1234ABCD"), and section
   15.3's own documented bring-up procedure is to read that line off one
   board and paste it into another's `link ping`. Rejecting the exact
   format the tool itself prints would fail this PR's own hardware
   acceptance test on a technicality nobody would think to work around. */
static bool parse_hex_u32(const char *tok, uint32_t *out)
{
  if (tok == NULL)
  {
    return false;
  }
  if (tok[0] == '0' && (tok[1] == 'x' || tok[1] == 'X'))
  {
    tok += 2;
  }
  size_t len = strlen(tok);
  if (len == 0u || len > 8u)
  {
    return false;
  }

  uint32_t value = 0;
  for (size_t i = 0; i < len; i++)
  {
    char     c = tok[i];
    uint32_t digit;
    if (c >= '0' && c <= '9')
    {
      digit = (uint32_t)(c - '0');
    }
    else if (c >= 'a' && c <= 'f')
    {
      digit = (uint32_t)(c - 'a') + 10u;
    }
    else if (c >= 'A' && c <= 'F')
    {
      digit = (uint32_t)(c - 'A') + 10u;
    }
    else
    {
      return false;
    }
    value = (value << 4) | digit;
  }
  *out = value;
  return true;
}

/* `link ping` is deliberately just boomlink_link_send() with an ACK
   requested - the "pong" IS section 9.6's own ACK, which link_on_tx_done()
   above reports as BOOMLINK_TX_ACKED. No separate application-level pong is
   needed to prove the round trip: an ACK is the receiving node's link
   engine, not a human at a keyboard, replying. */
static void cmd_link(EmbeddedCli *cli, char *args, void *context)
{
  (void)context;
  const char *sub = embeddedCliGetToken(args, 1);

  if (sub != NULL && strcmp(sub, "status") == 0)
  {
    print_link_status(cli);
    return;
  }

  if (sub != NULL && strcmp(sub, "enable") == 0)
  {
    link_service_set_enabled(true);
    embeddedCliPrint(cli, "link: enabled (owns radio RX)");
    return;
  }

  if (sub != NULL && strcmp(sub, "disable") == 0)
  {
    link_service_set_enabled(false);
    embeddedCliPrint(cli, "link: disabled (radio RX handed back to `radio ping`'s raw preview)");
    return;
  }

  if (sub != NULL && strcmp(sub, "ping") == 0)
  {
    boomlink_link_t *link = link_service_link();
    if (link == NULL)
    {
      embeddedCliPrint(cli, "link: not initialized");
      return;
    }
    if (!link_service_enabled())
    {
      /* Not a hard error - the frame WOULD queue (boomlink_link_send()
         doesn't know about this flag) - but link_service_process() skips
         boomlink_link_poll() entirely while disabled, so it would sit
         queued and never transmit, with the operator watching for a
         `link tx done` line that can never arrive. Refusing here, rather
         than queueing something already known to be doomed, is the
         honest answer. */
      embeddedCliPrint(cli, "link: disabled - `link ping` would queue but never transmit "
                            "(see `link enable`)");
      return;
    }

    uint32_t dest;
    if (!parse_hex_u32(embeddedCliGetToken(args, 2), &dest))
    {
      embeddedCliPrint(cli, "usage: link ping <node_id_hex> [text]");
      return;
    }

    const char *payload = embeddedCliGetToken(args, 3);
    if (payload == NULL)
    {
      payload = "PING";
    }

    boomlink_link_send_result_t rc = boomlink_link_send(
        link, dest, BOOMLINK_TXPRIO_NORMAL, true, (const uint8_t *)payload, strlen(payload));

    char line[96];
    snprintf(line, sizeof(line), "link ping: %s (watch for a `link tx done` line)",
             send_result_name(rc));
    embeddedCliPrint(cli, line);
    return;
  }

  embeddedCliPrint(cli, "usage: link status | link enable | link disable | link ping <node_id_hex> [text]");
}

static const char *device_type_name(boomlink_DeviceType type)
{
  switch (type)
  {
    case boomlink_DeviceType_DEVICE_TYPE_STMNODE: return "stmnode";
    default:                                      return "unspecified";
  }
}

/* protocol_service_wakeup_response_fn - prints each section 8.6
   WakeupResponse this node collects after `wakeup <window_s>` below
   broadcasts a WakeupRequest. Registered once from cli_init(), after
   protocol_service_init() - see protocol_service_set_wakeup_response_
   callback()'s own doc for why this file, not protocol_service.c, owns the
   actual printing (the same reason link_on_rx()/link_on_tx_done() above do). */
static void wakeup_on_response(uint32_t source_id, const boomlink_WakeupResponse *response)
{
  if (s_cli == NULL || response == NULL)
  {
    return;
  }

  char line[96];
  snprintf(line, sizeof(line), "wakeup: 0x%08lX responded (%s, fw v%lu.%lu.%lu)",
           (unsigned long)source_id, device_type_name(response->device_type),
           (unsigned long)response->fw_version_major, (unsigned long)response->fw_version_minor,
           (unsigned long)response->fw_version_patch);
  embeddedCliPrint(s_cli, line);
}

/* Broadcasts a section 8.6 WakeupRequest asking every reachable node to
   report in within `window_s` seconds, each after its own randomly-drawn
   delay (ALOHA-style collision avoidance - see boomlink.md section 8.6).
   Each reply prints via wakeup_on_response() above as it arrives, not
   collected into a list here - there is no fixed count of nodes to wait
   for, and `window_s` is only how long the responding nodes wait, not a
   deadline this command itself blocks on. `wakeup 0` is valid (every node
   fires back immediately, with no random spread to avoid collisions) - just
   not useful for that purpose with more than one reachable node. */
static void cmd_wakeup(EmbeddedCli *cli, char *args, void *context)
{
  (void)context;
  if (embeddedCliGetTokenCount(args) < 1)
  {
    embeddedCliPrint(cli, "usage: wakeup <window_s> (0..3600)");
    return;
  }
  const char   *tok      = embeddedCliGetToken(args, 1);
  char         *end      = NULL;
  unsigned long window_s = strtoul(tok, &end, 10);
  /* The upper bound doubles as the strtoul-accepts-a-leading-'-' guard
     parse_hex_u32()'s own comment warns about elsewhere in this file: a
     negative window_s wraps to a huge unsigned value well past this check,
     not a small one, so it is rejected by the SAME range check rather than
     needing its own sign detection. BOOMLINK_SYSTEM_WAKEUP_MAX_WINDOW_S is
     the identical ceiling boomlink_system_service_arm_wakeup() clamps to
     anyway (review found window_s reaching that clamp unvalidated could
     overflow its internal *1000 arithmetic) - rejecting it here instead of
     silently letting it through and clamping tells the operator what
     actually happened, rather than a window quietly shorter than what they
     typed. */
  if (end == tok || window_s > BOOMLINK_SYSTEM_WAKEUP_MAX_WINDOW_S)
  {
    embeddedCliPrint(cli, "usage: wakeup <window_s> (0..3600)");
    return;
  }

  if (!link_service_enabled())
  {
    /* Same reasoning as `link ping` above: boomlink_link_send() would still
       queue the frame, but link_service_process() skips boomlink_link_poll()
       entirely while disabled, so it would sit queued and never transmit -
       refusing here is the honest answer, not queueing something already
       known to be doomed. */
    embeddedCliPrint(cli, "link: disabled - `wakeup` would queue but never transmit "
                          "(see `link enable`)");
    return;
  }

  char line[64];
  if (protocol_service_send_wakeup_request((uint32_t)window_s))
  {
    snprintf(line, sizeof(line), "wakeup: broadcast sent (window %lu s)", window_s);
  }
  else
  {
    snprintf(line, sizeof(line), "wakeup: failed to send (window %lu s)", window_s);
  }
  embeddedCliPrint(cli, line);
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
  /* 13 bindings after the BoomLink + detect/GPS merge (help is internal and
     not counted). embeddedCliAddBinding() silently drops the 11th+ command
     when this is too small - `link`/`proto`/`wakeup` vanished that way. */
  cfg->maxBindingCount   = 16;
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

  CliCommandBinding gps_binding = {
    .name         = "gps",
    .help         = "Stream <sec> seconds of raw NMEA from the GNSS module",
    .tokenizeArgs = true,
    .context      = NULL,
    .binding      = cmd_gps,
  };
  embeddedCliAddBinding(s_cli, gps_binding);

  CliCommandBinding gpstx_binding = {
    .name         = "gpstx",
    .help         = "Send one NMEA/$PSTM sentence to the GNSS module",
    .tokenizeArgs = true,
    .context      = NULL,
    .binding      = cmd_gpstx,
  };
  embeddedCliAddBinding(s_cli, gpstx_binding);

  CliCommandBinding micdiag_binding = {
    .name         = "micdiag",
    .help         = "PDM wiring diagnostics: toggle counts on D1/D2 + pull test",
    .tokenizeArgs = false,
    .context      = NULL,
    .binding      = cmd_micdiag,
  };
  embeddedCliAddBinding(s_cli, micdiag_binding);

  CliCommandBinding gpsrst_binding = {
    .name         = "gpsrst",
    .help         = "Pulse the GNSS module reset line (bring-up fallback)",
    .tokenizeArgs = false,
    .context      = NULL,
    .binding      = cmd_gpsrst,
  };
  embeddedCliAddBinding(s_cli, gpsrst_binding);

  CliCommandBinding dfu_binding = {
    .name         = "dfu",
    .help         = "Reboot into the ROM bootloader for USB DFU flashing",
    .tokenizeArgs = false,
    .context      = NULL,
    .binding      = cmd_dfu,
  };
  embeddedCliAddBinding(s_cli, dfu_binding);
  CliCommandBinding radio_binding = {
    .name         = "radio",
    .help         = "radio status | radio ping [text] | radio reset - raw SX1262 bring-up test "
                    "(RX preview needs `link disable`)",
    .tokenizeArgs = true,
    .context      = NULL,
    .binding      = cmd_radio,
  };
  embeddedCliAddBinding(s_cli, radio_binding);

  CliCommandBinding proto_binding = {
    .name         = "proto",
    .help         = "proto selftest - encode/decode a BoomProtocol Envelope round-trip (bring-up self-test)",
    .tokenizeArgs = true,
    .context      = NULL,
    .binding      = cmd_proto,
  };
  embeddedCliAddBinding(s_cli, proto_binding);

  CliCommandBinding link_binding = {
    .name         = "link",
    .help         = "link status | link enable | link disable | link ping <node_id_hex> [text]",
    .tokenizeArgs = true,
    .context      = NULL,
    .binding      = cmd_link,
  };
  embeddedCliAddBinding(s_cli, link_binding);

  CliCommandBinding wakeup_binding = {
    .name         = "wakeup",
    .help         = "wakeup <window_s> - broadcast a fleet discovery request (section 8.6)",
    .tokenizeArgs = true,
    .context      = NULL,
    .binding      = cmd_wakeup,
  };
  embeddedCliAddBinding(s_cli, wakeup_binding);

  /* PR 4 Phase C: load the persisted NodeConfig (or safe defaults - see
     protocol_service_load_config()'s own doc) BEFORE link_service_init(),
     so this node's configured identity (rather than only the UID-derived
     bring-up fallback link_service_init() still falls back to on its own)
     is what the link engine and the protocol dispatcher both come up
     against. */
  boomlink_node_config_t loaded_config;
  protocol_service_load_config(&loaded_config);

  /* Brings the link engine up against radio.h (App/link/link_service.h).
     Safe regardless of radio_init()'s own outcome - see link_service_init()'s
     doc: its port forwards to radio.h's singleton on every call rather than
     caching readiness, so this just means every send/poll is a no-op until
     the radio is (if ever). Not checking the return value: a config this
     file builds itself failing boomlink_link_init()'s validation would be a
     programming error to catch at review/test time, not a field condition
     to branch on - `link status` reports "not initialized" either way. */
  (void)link_service_init(loaded_config.general.node_id, loaded_config.link.magic, link_on_rx,
                          NULL, link_on_tx_done, NULL);

  /* link_service_init() may have just resolved an unconfigured (0 /
     BOOMLINK_ADDR_INVALID) node_id to its own UID-derived fallback - see
     that function's own doc. Feed the value it actually ended up running
     with back into loaded_config before handing it to protocol_service_
     init() below, so ConfigGet reports the address this node is
     demonstrably alive and answering on, not the "unconfigured" 0 every
     never-configured node's defaults() would otherwise still show. This is
     the canonical first thing an operator/provisioning tool asks a fresh
     node for, so getting it wrong here is not a corner case. */
  loaded_config.general.node_id = link_service_node_id();

  /* Wires the protocol dispatcher (command/config services) onto the link
     engine just brought up above - see protocol_service_init()'s own doc.
     Not checking the return value for the same reason as link_service_init()
     just above: the only way this fails is loaded_config being NULL, which
     the address-of-a-local-variable just taken cannot be. */
  (void)protocol_service_init(&loaded_config);

  /* Registered after protocol_service_init() - see protocol_service_set_
     wakeup_response_callback()'s own doc for why this ordering, and
     wakeup_on_response() above for what it prints. */
  protocol_service_set_wakeup_response_callback(wakeup_on_response);

  embeddedCliProcess(s_cli); /* print the initial prompt */
}

void cli_process(void)
{
  if (s_cli == NULL)
  {
    return;
  }
  embeddedCliProcess(s_cli);
  /* Drains RX (calling link_on_rx() for anything accepted) then services TX
     (calling link_on_tx_done() for anything a pending `link ping` just
     finished) - a no-op unless `link enable` currently owns radio_poll_rx(),
     same arbitration print_rx_frame() below observes from the other side. */
  link_service_process();
  /* PR 4 Phase C: drives the config service's revert-on-timeout poll and
     performs a `reboot` command's deferred reset once armed - see
     protocol_service_process()'s own doc. Independent of link_service_
     process()/print_rx_frame()'s enable/disable arbitration above: this
     does not touch the radio at all, so it runs every tick regardless. */
  protocol_service_process();
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

/**
 ******************************************************************************
 * @file    codec_tool.c
 * @brief   Host-native CLI wrapping boomlink_codec (see
 *          fw/common/boomlink/boomlink_codec.h): an internal round-trip
 *          self-test plus `encode`/`decode`/`limits` subcommands used as the
 *          "Nanopb side" of the cross-language interop tests in
 *          tests/test_encode_decode.py and tests/test_compatibility.py -
 *          boomlink.md section 15.1 requires "Python Protobuf encode ->
 *          Nanopb decode" and "Nanopb encode -> Python Protobuf decode" to
 *          both be exercised on the same test vectors, which needs a real
 *          Nanopb-side process pytest can shell out to.
 *
 *          Usage:
 *            codec_tool selftest
 *            codec_tool limits
 *            codec_tool encode <ping|pong> <protocol_version> \
 *                               <request_id> <payload_hex> <out_file>
 *            codec_tool decode <in_file>
 ******************************************************************************
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "boomlink_codec.h"

/* The real compiled payload bound per message, read from the generated
   struct layout itself rather than a hand-maintained copy of
   nanopb/system.options' max_size - changing that value here needs no
   corresponding edit anywhere in this file. */
#define BOOMLINK_PING_PAYLOAD_CAP (sizeof(((boomlink_Ping *)0)->payload.bytes))
#define BOOMLINK_PONG_PAYLOAD_CAP (sizeof(((boomlink_Pong *)0)->payload.bytes))
#define BOOMLINK_MAX_PAYLOAD_CAP \
  (BOOMLINK_PING_PAYLOAD_CAP > BOOMLINK_PONG_PAYLOAD_CAP ? BOOMLINK_PING_PAYLOAD_CAP \
                                                          : BOOMLINK_PONG_PAYLOAD_CAP)

/* Normally set by CMake (from the BOOMLINK_SANITIZE option) - defaulted here
   so this file still compiles standalone, e.g. in a hand-rolled reproduction
   build. Reported by `limits` as sanitizers=, see run_limits().
   The default is 0, which UNDER-reports a hand-rolled build that passed
   -fsanitize=... without also passing this -D. That direction is merely
   conservative; the dangerous direction is claiming instrumentation that
   isn't there, which would make a negative-path test's "clean rejection"
   verdict meaningless. GCC/Clang define __SANITIZE_ADDRESS__ under
   -fsanitize=address, so at least the ASan half of the claim can be
   cross-checked rather than trusted - there is no equivalent macro for UBSan,
   which is why the value has to come from the build system at all. */
#ifndef BOOMLINK_SANITIZE_ENABLED
#define BOOMLINK_SANITIZE_ENABLED 0
#endif

#if BOOMLINK_SANITIZE_ENABLED && !defined(__SANITIZE_ADDRESS__) && !defined(__clang__)
#error "BOOMLINK_SANITIZE_ENABLED=1 but this file is not compiled with -fsanitize=address"
#endif

/* `decode`'s read cap - see its own comment for why this is not simply
   boomlink_Envelope_size. */
#define BOOMLINK_DECODE_READ_CAP 512

/* The whole point of not sizing the read cap to boomlink_Envelope_size (see
   run_decode()'s comment) is that a forward-compatible frame from a newer
   peer can legitimately be larger than this schema's own worst case, right
   up to the real on-air budget - so the cap has to clear THE BUDGET, not
   merely boomlink_Envelope_size. Checking it against the latter (as an
   earlier version of this assert did) leaves the whole
   boomlink_Envelope_size..budget window unguarded: a cap of 220 compiles
   silently and then rejects a legitimate 235-byte frame at runtime, which
   is exactly the bug this cap was raised to fix.
   Additive form (cap + header >= max payload) rather than comparing against
   a "budget" subtraction, for the same unsigned-underflow reason spelled
   out in boomlink_codec.c's own budget assert. */
_Static_assert(BOOMLINK_DECODE_READ_CAP + BOOMLINK_LINK_FRAME_HEADER_SIZE >=
                   BOOMLINK_RADIO_MAX_PAYLOAD,
               "decode's read cap must clear the real on-air Envelope budget, not just "
               "boomlink_Envelope_size - see the comment above");

/* Parses `s` as a base-10 uint32_t. Rejects empty input, any leading
   character that isn't a digit (whitespace, '+', '-'), trailing junk, and
   anything out of uint32_t range.

   `strtoul`/`strtoull` alone accept far more than that: leading whitespace
   ("  5"), a leading '+' ("+5"), and - the sharpest edge - a leading '-'
   does NOT get rejected by the standard library. `strtoul("-1", ...)`
   successfully parses the entire string as a negated-then-wrapped
   unsigned value (`ULONG_MAX`), with `errno` left untouched, so a
   range-only check does not catch it either; the previous version of this
   function only rejected "-1" by accident, because wrapping happened to
   land above 0xFFFFFFFF on this build's 64-bit `unsigned long` - the exact
   same input would have been silently ACCEPTED as a large-but-in-range
   value on a platform where `unsigned long` is 32 bits. Requiring the
   first character to be '0'-'9' rejects all of the above in one check,
   portably.

   Uses `strtoull`/`unsigned long long` (guaranteed >= 64 bits by the C
   standard) rather than `strtoul`/`unsigned long` for the same portability
   reason: the `> 0xFFFFFFFFULL` range check needs a type wider than
   uint32_t to mean anything, and plain `unsigned long` is only 32 bits on
   some real platforms (e.g. Windows LLP64). */
static bool parse_u32(const char *s, uint32_t *out) {
  if (s == NULL || s[0] < '0' || s[0] > '9') {
    return false;
  }
  errno                    = 0;
  char              *end   = NULL;
  unsigned long long value = strtoull(s, &end, 10);
  if (*end != '\0' || errno == ERANGE || value > 0xFFFFFFFFULL) {
    return false;
  }
  *out = (uint32_t)value;
  return true;
}

static int hex_nibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

/* Decodes `hex` (even-length, no separators) into `out` (capacity
   `out_cap`). Returns the decoded length, or -1 on a malformed/oversized
   input. An empty string decodes to a zero-length payload. */
static int hex_decode(const char *hex, uint8_t *out, size_t out_cap) {
  size_t hex_len = strlen(hex);
  if (hex_len % 2 != 0) return -1;
  size_t out_len = hex_len / 2;
  if (out_len > out_cap) return -1;
  for (size_t i = 0; i < out_len; i++) {
    int hi = hex_nibble(hex[2 * i]);
    int lo = hex_nibble(hex[2 * i + 1]);
    if (hi < 0 || lo < 0) return -1;
    out[i] = (uint8_t)((hi << 4) | lo);
  }
  return (int)out_len;
}

/* `out` must have capacity >= 2*len+1. */
static void hex_encode(const uint8_t *data, size_t len, char *out, size_t out_cap) {
  static const char digits[] = "0123456789abcdef";
  if (out_cap < 2 * len + 1) {
    /* Should be unreachable: callers size `out` from BOOMLINK_MAX_PAYLOAD_CAP
       and `len` comes from a decoded payload, which boomlink_codec.c's
       BOOMLINK_NANOPB_ENFORCED_MAX asserts cannot exceed that cap. It is a
       real guard rather than a formality, though - it once WAS reachable, when
       an odd max_size let Nanopb accept one byte more than the declared array
       (see that assert), and it then failed the worst possible way: silently
       printing an empty payload with exit status 0, so `decode` reported
       success while losing the data. Fail loudly instead. */
    fprintf(stderr,
            "hex_encode: output buffer too small for a %zu-byte payload (capacity %zu) - "
            "this is a bug in codec_tool's buffer sizing, not a bad input\n",
            len, out_cap);
    exit(70); /* EX_SOFTWARE - not a rejection of the input, an internal fault */
  }
  for (size_t i = 0; i < len; i++) {
    out[2 * i]     = digits[(data[i] >> 4) & 0xF];
    out[2 * i + 1] = digits[data[i] & 0xF];
  }
  out[2 * len] = '\0';
}

/* Internal round-trip regression check, independent of any file I/O: builds
   a Ping-carrying Envelope, encodes it, decodes the bytes back, and checks
   every field survived. Exercised directly by CTest (no `decode`/`encode`
   subprocess plumbing needed for this one). */
static int run_selftest(void) {
  boomlink_Envelope envelope = boomlink_Envelope_init_zero;
  boomlink_envelope_init(&envelope);
  envelope.header.request_id            = 42;
  envelope.which_payload                = boomlink_Envelope_system_tag;
  envelope.payload.system.which_message = boomlink_SystemMessage_ping_tag;
  const uint8_t kPayload[]              = {0xDE, 0xAD, 0xBE, 0xEF};
  /* This hardcoded payload has no inherent relationship to the compiled
     Ping payload bound (unlike run_encode/run_decode below, which size
     everything from BOOMLINK_PING_PAYLOAD_CAP) - shrinking that bound
     enough would silently overflow the memcpy below. Caught here instead
     of at runtime: a build with too small a bound now fails to compile. */
  _Static_assert(BOOMLINK_PING_PAYLOAD_CAP >= sizeof(kPayload),
                 "selftest's fixed payload no longer fits the compiled Ping payload bound");
  memcpy(envelope.payload.system.message.ping.payload.bytes, kPayload, sizeof(kPayload));
  envelope.payload.system.message.ping.payload.size = sizeof(kPayload);

  uint8_t buf[boomlink_Envelope_size];
  size_t  len = 0;
  if (!boomlink_encode_envelope(&envelope, buf, sizeof(buf), &len)) {
    fprintf(stderr, "selftest: encode failed\n");
    return 1;
  }

  boomlink_Envelope decoded = boomlink_Envelope_init_zero;
  if (!boomlink_decode_envelope(buf, len, &decoded)) {
    fprintf(stderr, "selftest: decode failed\n");
    return 1;
  }

  if (decoded.header.protocol_version != BOOMLINK_PROTOCOL_VERSION || decoded.header.request_id != 42 ||
      decoded.which_payload != boomlink_Envelope_system_tag ||
      decoded.payload.system.which_message != boomlink_SystemMessage_ping_tag ||
      decoded.payload.system.message.ping.payload.size != sizeof(kPayload) ||
      memcmp(decoded.payload.system.message.ping.payload.bytes, kPayload, sizeof(kPayload)) != 0) {
    fprintf(stderr, "selftest: round-tripped envelope does not match the original\n");
    return 1;
  }

  /* A truncated encoding must fail closed, not decode a partial value. */
  boomlink_Envelope truncated = boomlink_Envelope_init_zero;
  if (len > 1 && boomlink_decode_envelope(buf, len - 1, &truncated)) {
    fprintf(stderr, "selftest: truncated input unexpectedly decoded successfully\n");
    return 1;
  }

  /* An Envelope with protocol_version left at 0 must be rejected by the
     encoder, not silently produce a wire-valid frame nobody asked for. */
  boomlink_Envelope zero_version = boomlink_Envelope_init_zero;
  zero_version.has_header        = true; /* protocol_version stays 0 */
  zero_version.which_payload     = boomlink_Envelope_system_tag;
  uint8_t reject_buf[boomlink_Envelope_size];
  size_t  reject_len = 0;
  if (boomlink_encode_envelope(&zero_version, reject_buf, sizeof(reject_buf), &reject_len)) {
    fprintf(stderr, "selftest: encoding protocol_version=0 unexpectedly succeeded\n");
    return 1;
  }

  printf("selftest: ok\n");
  return 0;
}

/* Prints the real compiled Ping/Pong payload bounds and the real on-air
   Envelope budget, so tests read every one of these from this tool instead
   of each hardcoding its own copy - envelope_size is Nanopb's own
   worst-case encoded size (envelope.pb.h's boomlink_Envelope_size, already
   used to size buf[] throughout this file); envelope_budget is the real
   on-air ceiling (boomlink_codec.h's BOOMLINK_RADIO_MAX_PAYLOAD minus
   BOOMLINK_LINK_FRAME_HEADER_SIZE) a valid frame from a newer peer could
   still legitimately approach, per BOOMLINK_DECODE_READ_CAP's own comment
   above. */
static int run_limits(void) {
  printf("ping_payload_max=%zu\n", BOOMLINK_PING_PAYLOAD_CAP);
  printf("pong_payload_max=%zu\n", BOOMLINK_PONG_PAYLOAD_CAP);
  printf("envelope_size=%u\n", (unsigned)boomlink_Envelope_size);
  /* Both operands are unsigned, so this subtraction would wrap rather than
     go negative if the header were ever larger than the max payload. It
     cannot be, in any binary that compiled: boomlink_codec.c's budget
     assert requires BOOMLINK_RADIO_MAX_PAYLOAD >=
     BOOMLINK_LINK_FRAME_HEADER_SIZE + boomlink_Envelope_size, and this tool
     links boomlink_protocol. Spelled out because the same subtraction was a
     real (if latent) bug elsewhere in this package - see cli.c's identical
     note. */
  printf("envelope_budget=%u\n",
         (unsigned)(BOOMLINK_RADIO_MAX_PAYLOAD - BOOMLINK_LINK_FRAME_HEADER_SIZE));
  printf("decode_read_cap=%d\n", BOOMLINK_DECODE_READ_CAP);
  /* Set from CMake (BOOMLINK_SANITIZE) rather than sniffed here: GCC defines
     __SANITIZE_ADDRESS__ for ASan but offers no UBSan equivalent, so half the
     answer would be unknowable from inside the code. Reported so the pytest
     suite can tell the difference between "this rejection was clean" and
     "there was no sanitizer watching" - see tests/conftest.py. */
  printf("sanitizers=%d\n", BOOMLINK_SANITIZE_ENABLED);
  return 0;
}

static int run_encode(int argc, char **argv) {
  if (argc != 7) {
    fprintf(stderr,
            "usage: encode <ping|pong> <protocol_version> <request_id> <payload_hex> <out_file>\n");
    return 2;
  }
  const char *kind        = argv[2];
  uint32_t    protocol_version;
  uint32_t    request_id;
  if (!parse_u32(argv[3], &protocol_version)) {
    fprintf(stderr, "encode: '%s' is not a valid uint32 protocol_version\n", argv[3]);
    return 2;
  }
  if (!parse_u32(argv[4], &request_id)) {
    fprintf(stderr, "encode: '%s' is not a valid uint32 request_id\n", argv[4]);
    return 2;
  }
  const char *payload_hex = argv[5];
  const char *out_path    = argv[6];

  boomlink_Envelope envelope = boomlink_Envelope_init_zero;
  boomlink_envelope_init(&envelope);
  envelope.header.protocol_version = protocol_version; /* may deliberately be 0, to test rejection */
  envelope.header.request_id       = request_id;
  envelope.which_payload           = boomlink_Envelope_system_tag;

  uint8_t   *payload_bytes;
  pb_size_t *payload_size;
  size_t     payload_cap;
  if (strcmp(kind, "ping") == 0) {
    envelope.payload.system.which_message = boomlink_SystemMessage_ping_tag;
    payload_bytes                         = envelope.payload.system.message.ping.payload.bytes;
    payload_size                          = &envelope.payload.system.message.ping.payload.size;
    payload_cap                           = BOOMLINK_PING_PAYLOAD_CAP;
  } else if (strcmp(kind, "pong") == 0) {
    envelope.payload.system.which_message = boomlink_SystemMessage_pong_tag;
    payload_bytes                         = envelope.payload.system.message.pong.payload.bytes;
    payload_size                          = &envelope.payload.system.message.pong.payload.size;
    payload_cap                           = BOOMLINK_PONG_PAYLOAD_CAP;
  } else {
    fprintf(stderr, "encode: unknown kind '%s' (expected ping or pong)\n", kind);
    return 2;
  }

  int decoded_len = hex_decode(payload_hex, payload_bytes, payload_cap);
  if (decoded_len < 0) {
    fprintf(stderr, "encode: malformed or oversized payload_hex\n");
    return 2;
  }
  *payload_size = (pb_size_t)decoded_len;

  uint8_t buf[boomlink_Envelope_size];
  size_t  len = 0;
  if (!boomlink_encode_envelope(&envelope, buf, sizeof(buf), &len)) {
    fprintf(stderr, "encode: boomlink_encode_envelope failed\n");
    return 1;
  }

  FILE *f = fopen(out_path, "wb");
  if (f == NULL) {
    fprintf(stderr, "encode: could not open '%s' for writing\n", out_path);
    return 1;
  }
  size_t written = fwrite(buf, 1, len, f);
  fclose(f);
  if (written != len) {
    fprintf(stderr, "encode: short write to '%s'\n", out_path);
    return 1;
  }
  return 0;
}

static int run_decode(int argc, char **argv) {
  if (argc != 3) {
    fprintf(stderr, "usage: decode <in_file>\n");
    return 2;
  }
  const char *in_path = argv[2];

  FILE *f = fopen(in_path, "rb");
  if (f == NULL) {
    fprintf(stderr, "decode: could not open '%s' for reading\n", in_path);
    return 1;
  }
  /* Deliberately NOT sized to exactly boomlink_Envelope_size: that is only
     this build's own schema's worst case, not a ceiling on what a valid
     frame from a newer peer could look like - a message carrying a field
     this schema doesn't recognize yet (section 15.1's "unknown fields for
     forward compatibility" rule) can legitimately be larger, and rejecting
     it here - before boomlink_decode_envelope() ever sees it - would make
     forward compatibility impossible to exercise through this tool.
     BOOMLINK_DECODE_READ_CAP is generously above the real on-air ceiling
     (BOOMLINK_RADIO_MAX_PAYLOAD minus BOOMLINK_LINK_FRAME_HEADER_SIZE, both
     from boomlink_codec.h - the `limits` subcommand reports the computed
     value as envelope_budget) instead, enforced by the _Static_assert at
     the top of this file.

     Reads one byte past that cap so an oversized file is detected by "we
     read more than the cap allows" rather than by feof() after a read
     that exactly fills a same-sized buffer - fread() does not set EOF in
     that case, which is why the previous version of this cap rejected a
     legitimately-sized input of precisely boomlink_Envelope_size bytes as
     "too large". */
  uint8_t buf[BOOMLINK_DECODE_READ_CAP + 1];
  size_t  len  = fread(buf, 1, sizeof(buf), f);
  int     ferr = ferror(f);
  fclose(f);
  if (ferr) {
    fprintf(stderr, "decode: read error on '%s'\n", in_path);
    return 1;
  }
  if (len > BOOMLINK_DECODE_READ_CAP) {
    fprintf(stderr, "decode: '%s' exceeds the maximum test input size (%d bytes)\n", in_path,
            BOOMLINK_DECODE_READ_CAP);
    return 1;
  }

  boomlink_Envelope envelope = boomlink_Envelope_init_zero;
  if (!boomlink_decode_envelope(buf, len, &envelope)) {
    fprintf(stderr, "decode: malformed input\n");
    return 1;
  }

  printf("protocol_version=%u\n", (unsigned)envelope.header.protocol_version);
  printf("request_id=%u\n", (unsigned)envelope.header.request_id);

  if (envelope.which_payload != boomlink_Envelope_system_tag) {
    printf("kind=none\n");
    return 0;
  }

  const boomlink_SystemMessage *system = &envelope.payload.system;
  char                          hex[2 * BOOMLINK_MAX_PAYLOAD_CAP + 1];
  if (system->which_message == boomlink_SystemMessage_ping_tag) {
    hex_encode(system->message.ping.payload.bytes, system->message.ping.payload.size, hex,
               sizeof(hex));
    printf("kind=ping\n");
    printf("payload=%s\n", hex);
  } else if (system->which_message == boomlink_SystemMessage_pong_tag) {
    hex_encode(system->message.pong.payload.bytes, system->message.pong.payload.size, hex,
               sizeof(hex));
    printf("kind=pong\n");
    printf("payload=%s\n", hex);
  } else {
    printf("kind=system_unknown\n");
  }
  return 0;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: %s <selftest|limits|encode|decode> [args...]\n", argv[0]);
    return 2;
  }
  if (strcmp(argv[1], "selftest") == 0) {
    return run_selftest();
  }
  if (strcmp(argv[1], "limits") == 0) {
    return run_limits();
  }
  if (strcmp(argv[1], "encode") == 0) {
    return run_encode(argc, argv);
  }
  if (strcmp(argv[1], "decode") == 0) {
    return run_decode(argc, argv);
  }
  fprintf(stderr, "unknown subcommand '%s'\n", argv[1]);
  return 2;
}

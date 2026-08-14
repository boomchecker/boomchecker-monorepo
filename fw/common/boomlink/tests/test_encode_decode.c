/**
 ******************************************************************************
 * @file    test_encode_decode.c
 * @brief   Host-native C test harness for boomlink_codec (see
 *          fw/common/boomlink/boomlink_codec.h) and standalone `encode`/
 *          `decode` CLI subcommands used as the "Nanopb side" of the
 *          cross-language interop tests in tests/test_encode_decode.py -
 *          boomlink.md section 15.1 requires "Python Protobuf encode ->
 *          Nanopb decode" and "Nanopb encode -> Python Protobuf decode" to
 *          both be exercised on the same test vectors, which needs a real
 *          Nanopb-side process pytest can shell out to.
 *
 *          Usage:
 *            test_encode_decode selftest
 *            test_encode_decode encode <ping|pong> <protocol_version> \
 *                                <request_id> <payload_hex> <out_file>
 *            test_encode_decode decode <in_file>
 ******************************************************************************
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "boomlink_codec.h"

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

static void hex_encode(const uint8_t *data, size_t len, char *out /* >= 2*len+1 */) {
  static const char digits[] = "0123456789abcdef";
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
  boomlink_Envelope envelope  = boomlink_Envelope_init_zero;
  /* header is a singular message-type field, so proto3 tracks its presence
     explicitly (has_header) rather than always encoding it - unlike a
     top-level scalar field, setting sub-fields alone does not imply
     presence. Forgetting this line encodes an Envelope with NO header at
     all: protocol_version/request_id silently decode as 0 on the other end
     instead of a build/runtime error, since it is a fully valid (if empty)
     encoding of the schema. */
  envelope.has_header               = true;
  envelope.header.protocol_version = 1;
  envelope.header.request_id       = 42;
  envelope.which_payload           = boomlink_Envelope_system_tag;
  envelope.payload.system.which_message = boomlink_SystemMessage_ping_tag;
  const uint8_t kPayload[] = {0xDE, 0xAD, 0xBE, 0xEF};
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

  if (decoded.header.protocol_version != 1 || decoded.header.request_id != 42 ||
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

  printf("selftest: ok\n");
  return 0;
}

static int run_encode(int argc, char **argv) {
  if (argc != 7) {
    fprintf(stderr,
            "usage: encode <ping|pong> <protocol_version> <request_id> <payload_hex> <out_file>\n");
    return 2;
  }
  const char *kind             = argv[2];
  uint32_t    protocol_version = (uint32_t)strtoul(argv[3], NULL, 10);
  uint32_t    request_id       = (uint32_t)strtoul(argv[4], NULL, 10);
  const char *payload_hex      = argv[5];
  const char *out_path         = argv[6];

  boomlink_Envelope envelope       = boomlink_Envelope_init_zero;
  envelope.has_header               = true; /* see selftest's comment on has_header */
  envelope.header.protocol_version = protocol_version;
  envelope.header.request_id       = request_id;
  envelope.which_payload           = boomlink_Envelope_system_tag;

  uint8_t   *payload_bytes;
  pb_size_t *payload_size;
  if (strcmp(kind, "ping") == 0) {
    envelope.payload.system.which_message = boomlink_SystemMessage_ping_tag;
    payload_bytes = envelope.payload.system.message.ping.payload.bytes;
    payload_size  = &envelope.payload.system.message.ping.payload.size;
  } else if (strcmp(kind, "pong") == 0) {
    envelope.payload.system.which_message = boomlink_SystemMessage_pong_tag;
    payload_bytes = envelope.payload.system.message.pong.payload.bytes;
    payload_size  = &envelope.payload.system.message.pong.payload.size;
  } else {
    fprintf(stderr, "encode: unknown kind '%s' (expected ping or pong)\n", kind);
    return 2;
  }

  int decoded_len = hex_decode(payload_hex, payload_bytes, 64);
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
  uint8_t buf[512];
  size_t  len = fread(buf, 1, sizeof(buf), f);
  int     eof = feof(f);
  fclose(f);
  if (!eof) {
    fprintf(stderr, "decode: '%s' is larger than the maximum expected envelope size\n", in_path);
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
  char hex[2 * 64 + 1];
  if (system->which_message == boomlink_SystemMessage_ping_tag) {
    hex_encode(system->message.ping.payload.bytes, system->message.ping.payload.size, hex);
    printf("kind=ping\n");
    printf("payload=%s\n", hex);
  } else if (system->which_message == boomlink_SystemMessage_pong_tag) {
    hex_encode(system->message.pong.payload.bytes, system->message.pong.payload.size, hex);
    printf("kind=pong\n");
    printf("payload=%s\n", hex);
  } else {
    printf("kind=system_unknown\n");
  }
  return 0;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: %s <selftest|encode|decode> [args...]\n", argv[0]);
    return 2;
  }
  if (strcmp(argv[1], "selftest") == 0) {
    return run_selftest();
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

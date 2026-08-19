/**
 ******************************************************************************
 * @file    linkframe_tool.c
 * @brief   Host-native CLI wrapping boomlink_linkframe (see
 *          fw/common/boomlink/linkframe/boomlink_linkframe.h), so the pytest
 *          suite can cross-check the C parser/encoder against the independent
 *          Python reference implementation
 *          (linkframe/boomlink_linkframe.py) on the same wire bytes - the same
 *          arrangement boomlink.md section 15.1 requires for the Protobuf
 *          codec, applied to section 7.3's hand-packed header.
 *
 *          Usage:
 *            linkframe_tool selftest
 *            linkframe_tool limits
 *            linkframe_tool encode <frame_type> <flags> <fragment_index> \
 *                                  <dest> <src> <session> <sequence> \
 *                                  <payload_hex> <out_file>
 *            linkframe_tool parse <in_file> [expected_magic]
 *            linkframe_tool accepts <destination_id> <local_node_id>
 *
 *          `encode` takes raw flags/fragment_index bytes rather than named
 *          booleans deliberately: the tests need to build frames that a
 *          correct sender would never produce (a reserved flag bit set, a
 *          non-zero fragment_index) to check the receiver drops or ignores
 *          them per section 7.3.
 ******************************************************************************
 */
#include <stdio.h>
#include <string.h>

#include "boomlink_linkframe.h"
#include "tool_support.h"

/* Payload cap for this tool's buffers. The link layer itself imposes no
   payload bound - it treats the payload as opaque bytes and the radio's
   RADIO_MAX_PAYLOAD is what limits a real frame - so this is purely a test
   harness limit, sized well past the 255-byte SX126x packet ceiling so a test
   can build an oversized frame and watch it be rejected. */
#define LINKFRAME_TOOL_MAX_PAYLOAD 512u

static int run_selftest(void) {
  /* Round-trip every field through encode -> parse, with values chosen so a
     swapped or truncated field cannot go unnoticed: each 32-bit field gets a
     distinct byte pattern rather than a small integer, so mixing up
     destination and source, or dropping the high bytes, changes the result. */
  boomlink_linkframe_header_t header = {
      .magic          = BOOMLINK_LINKFRAME_MAGIC_DEFAULT,
      .version        = BOOMLINK_LINKFRAME_VERSION,
      .frame_type     = BOOMLINK_FRAME_TYPE_DATA,
      .ack_requested  = true,
      .more_fragments = false,
      .fragment_index = 0u,
      .destination_id = 0x11223344u,
      .source_id      = 0x55667788u,
      .session_id     = 0x99AABBCCu,
      .sequence       = 0xDDEEFF01u,
  };

  uint8_t buf[BOOMLINK_LINKFRAME_HEADER_SIZE + 4u];
  boomlink_linkframe_encode(&header, buf);
  const uint8_t kPayload[] = {0xDE, 0xAD, 0xBE, 0xEF};
  memcpy(&buf[BOOMLINK_LINKFRAME_HEADER_SIZE], kPayload, sizeof(kPayload));

  boomlink_linkframe_header_t       decoded     = {0};
  size_t                           payload_len = 0u;
  boomlink_linkframe_parse_result_t result      = boomlink_linkframe_parse(
      buf, sizeof(buf), BOOMLINK_LINKFRAME_MAGIC_DEFAULT, &decoded, &payload_len);
  if (result != BOOMLINK_LINKFRAME_OK) {
    fprintf(stderr, "selftest: parse failed: %s\n",
            boomlink_linkframe_parse_result_str(result));
    return 1;
  }
  if (decoded.magic != header.magic || decoded.version != header.version ||
      decoded.frame_type != header.frame_type ||
      decoded.ack_requested != header.ack_requested ||
      decoded.more_fragments != header.more_fragments ||
      decoded.fragment_index != header.fragment_index ||
      decoded.destination_id != header.destination_id ||
      decoded.source_id != header.source_id || decoded.session_id != header.session_id ||
      decoded.sequence != header.sequence) {
    fprintf(stderr, "selftest: round-tripped header does not match the original\n");
    return 1;
  }
  if (payload_len != sizeof(kPayload) ||
      memcmp(&buf[BOOMLINK_LINKFRAME_HEADER_SIZE], kPayload, sizeof(kPayload)) != 0) {
    fprintf(stderr, "selftest: payload did not survive the round-trip\n");
    return 1;
  }

  /* A truncated header must fail closed rather than read past the buffer. */
  boomlink_linkframe_header_t truncated = {0};
  size_t                     ignored   = 0u;
  if (boomlink_linkframe_parse(buf, BOOMLINK_LINKFRAME_HEADER_SIZE - 1u,
                               BOOMLINK_LINKFRAME_MAGIC_DEFAULT, &truncated,
                               &ignored) != BOOMLINK_LINKFRAME_ERR_TOO_SHORT) {
    fprintf(stderr, "selftest: a short buffer was not rejected\n");
    return 1;
  }

  printf("selftest: ok\n");
  return 0;
}

/* The compile-time constants the tests would otherwise hardcode a copy of -
   same discipline as codec_tool's `limits`. */
static int run_limits(void) {
  printf("header_size=%u\n", (unsigned)BOOMLINK_LINKFRAME_HEADER_SIZE);
  printf("magic_default=%u\n", (unsigned)BOOMLINK_LINKFRAME_MAGIC_DEFAULT);
  printf("version=%u\n", (unsigned)BOOMLINK_LINKFRAME_VERSION);
  printf("frame_type_data=%u\n", (unsigned)BOOMLINK_FRAME_TYPE_DATA);
  printf("frame_type_ack=%u\n", (unsigned)BOOMLINK_FRAME_TYPE_ACK);
  printf("addr_invalid=%u\n", (unsigned)BOOMLINK_ADDR_INVALID);
  printf("addr_broadcast=%u\n", (unsigned)BOOMLINK_ADDR_BROADCAST);
  printf("flag_ack_requested=%u\n", (unsigned)BOOMLINK_LINKFRAME_FLAG_ACK_REQUESTED);
  printf("flag_more_fragments=%u\n", (unsigned)BOOMLINK_LINKFRAME_FLAG_MORE_FRAGMENTS);
  printf("flags_reserved_mask=%u\n", (unsigned)BOOMLINK_LINKFRAME_FLAGS_RESERVED_MASK);
  printf("max_payload=%u\n", (unsigned)LINKFRAME_TOOL_MAX_PAYLOAD);
  /* Every parse result code, so the Python ParseResult enum is checked against
     the C enumerators rather than assumed to match. */
  printf("result_ok=%d\n", (int)BOOMLINK_LINKFRAME_OK);
  printf("result_too_short=%d\n", (int)BOOMLINK_LINKFRAME_ERR_TOO_SHORT);
  printf("result_magic=%d\n", (int)BOOMLINK_LINKFRAME_ERR_MAGIC);
  printf("result_version=%d\n", (int)BOOMLINK_LINKFRAME_ERR_VERSION);
  printf("result_frame_type=%d\n", (int)BOOMLINK_LINKFRAME_ERR_FRAME_TYPE);
  printf("result_fragmented=%d\n", (int)BOOMLINK_LINKFRAME_ERR_FRAGMENTED);
  printf("result_ack_has_payload=%d\n", (int)BOOMLINK_LINKFRAME_ERR_ACK_HAS_PAYLOAD);
  return 0;
}

static int run_encode(int argc, char **argv) {
  if (argc != 11) {
    fprintf(stderr,
            "usage: encode <frame_type> <flags> <fragment_index> <dest> <src> <session> "
            "<sequence> <payload_hex> <out_file>\n");
    return 2;
  }
  uint8_t  frame_type;
  uint8_t  flags;
  uint8_t  fragment_index;
  uint32_t dest;
  uint32_t src;
  uint32_t session;
  uint32_t sequence;
  /* One message per bad argument naming which one it was: a tool that just
     says "bad input" makes a typo'd test look like a protocol failure. */
  if (!boomlink_tool_parse_u8(argv[2], &frame_type)) {
    fprintf(stderr, "encode: '%s' is not a valid uint8 frame_type\n", argv[2]);
    return 2;
  }
  if (!boomlink_tool_parse_u8(argv[3], &flags)) {
    fprintf(stderr, "encode: '%s' is not a valid uint8 flags\n", argv[3]);
    return 2;
  }
  if (!boomlink_tool_parse_u8(argv[4], &fragment_index)) {
    fprintf(stderr, "encode: '%s' is not a valid uint8 fragment_index\n", argv[4]);
    return 2;
  }
  if (!boomlink_tool_parse_u32(argv[5], &dest)) {
    fprintf(stderr, "encode: '%s' is not a valid uint32 destination_id\n", argv[5]);
    return 2;
  }
  if (!boomlink_tool_parse_u32(argv[6], &src)) {
    fprintf(stderr, "encode: '%s' is not a valid uint32 source_id\n", argv[6]);
    return 2;
  }
  if (!boomlink_tool_parse_u32(argv[7], &session)) {
    fprintf(stderr, "encode: '%s' is not a valid uint32 session_id\n", argv[7]);
    return 2;
  }
  if (!boomlink_tool_parse_u32(argv[8], &sequence)) {
    fprintf(stderr, "encode: '%s' is not a valid uint32 sequence\n", argv[8]);
    return 2;
  }
  const char *payload_hex = argv[9];
  const char *out_path    = argv[10];

  uint8_t payload[LINKFRAME_TOOL_MAX_PAYLOAD];
  int     payload_len = boomlink_tool_hex_decode(payload_hex, payload, sizeof(payload));
  if (payload_len < 0) {
    fprintf(stderr, "encode: malformed or oversized payload_hex\n");
    return 2;
  }

  /* Built field by field rather than from the flags byte, then the raw flags
     are written back over the encoder's output below - see the comment there. */
  boomlink_linkframe_header_t header = {
      .magic          = BOOMLINK_LINKFRAME_MAGIC_DEFAULT,
      .version        = BOOMLINK_LINKFRAME_VERSION,
      .frame_type     = frame_type,
      .ack_requested  = (flags & BOOMLINK_LINKFRAME_FLAG_ACK_REQUESTED) != 0u,
      .more_fragments = (flags & BOOMLINK_LINKFRAME_FLAG_MORE_FRAGMENTS) != 0u,
      .fragment_index = fragment_index,
      .destination_id = dest,
      .source_id      = src,
      .session_id     = session,
      .sequence       = sequence,
  };

  uint8_t buf[BOOMLINK_LINKFRAME_HEADER_SIZE + LINKFRAME_TOOL_MAX_PAYLOAD];
  boomlink_linkframe_encode(&header, buf);
  /* boomlink_linkframe_encode() correctly refuses to put reserved flag bits on
     the air, so writing the raw byte over its output is the only way to build
     the "a newer sender set a bit we don't know" frame the reserved-bits
     receiver rule has to be tested against. Done here, in the test tool, and
     deliberately not by weakening the encoder. */
  buf[2] = flags;
  if (payload_len > 0) {
    memcpy(&buf[BOOMLINK_LINKFRAME_HEADER_SIZE], payload, (size_t)payload_len);
  }
  const size_t frame_len = BOOMLINK_LINKFRAME_HEADER_SIZE + (size_t)payload_len;

  FILE *f = fopen(out_path, "wb");
  if (f == NULL) {
    fprintf(stderr, "encode: could not open '%s' for writing\n", out_path);
    return 1;
  }
  size_t written = fwrite(buf, 1, frame_len, f);
  fclose(f);
  if (written != frame_len) {
    fprintf(stderr, "encode: short write to '%s'\n", out_path);
    return 1;
  }
  return 0;
}

static int run_parse(int argc, char **argv) {
  if (argc != 3 && argc != 4) {
    fprintf(stderr, "usage: parse <in_file> [expected_magic]\n");
    return 2;
  }
  const char *in_path        = argv[2];
  uint8_t     expected_magic = BOOMLINK_LINKFRAME_MAGIC_DEFAULT;
  if (argc == 4 && !boomlink_tool_parse_u8(argv[3], &expected_magic)) {
    fprintf(stderr, "parse: '%s' is not a valid uint8 expected_magic\n", argv[3]);
    return 2;
  }

  FILE *f = fopen(in_path, "rb");
  if (f == NULL) {
    fprintf(stderr, "parse: could not open '%s' for reading\n", in_path);
    return 1;
  }
  /* One byte past the cap, so an oversized input is detected by having read
     more than allowed rather than by feof() after a read that exactly filled
     the buffer - fread() does not set EOF in that case. */
  uint8_t buf[BOOMLINK_LINKFRAME_HEADER_SIZE + LINKFRAME_TOOL_MAX_PAYLOAD + 1u];
  size_t  len  = fread(buf, 1, sizeof(buf), f);
  int     ferr = ferror(f);
  fclose(f);
  if (ferr) {
    fprintf(stderr, "parse: read error on '%s'\n", in_path);
    return 1;
  }
  if (len > BOOMLINK_LINKFRAME_HEADER_SIZE + LINKFRAME_TOOL_MAX_PAYLOAD) {
    fprintf(stderr, "parse: '%s' exceeds the maximum test input size (%u bytes)\n", in_path,
            (unsigned)(BOOMLINK_LINKFRAME_HEADER_SIZE + LINKFRAME_TOOL_MAX_PAYLOAD));
    return 1;
  }

  boomlink_linkframe_header_t header      = {0};
  size_t                      payload_len = 0u;
  boomlink_linkframe_parse_result_t result =
      boomlink_linkframe_parse(buf, len, expected_magic, &header, &payload_len);

  /* The result code is printed on BOTH paths, and the process exits 0 even for
     a rejected frame: a rejection is this subcommand's normal, expected output
     (the tests assert on specific reasons), not a failure of the tool. Only an
     I/O or usage problem is a nonzero exit. */
  printf("result=%d\n", (int)result);
  printf("result_str=%s\n", boomlink_linkframe_parse_result_str(result));
  if (result != BOOMLINK_LINKFRAME_OK) {
    return 0;
  }

  printf("magic=%u\n", (unsigned)header.magic);
  printf("version=%u\n", (unsigned)header.version);
  printf("frame_type=%u\n", (unsigned)header.frame_type);
  printf("ack_requested=%u\n", header.ack_requested ? 1u : 0u);
  printf("more_fragments=%u\n", header.more_fragments ? 1u : 0u);
  printf("fragment_index=%u\n", (unsigned)header.fragment_index);
  printf("reserved_flags=%u\n", (unsigned)header.reserved_flags);
  printf("destination_id=%u\n", (unsigned)header.destination_id);
  printf("source_id=%u\n", (unsigned)header.source_id);
  printf("session_id=%u\n", (unsigned)header.session_id);
  printf("sequence=%u\n", (unsigned)header.sequence);
  printf("payload_len=%zu\n", payload_len);

  char hex[2u * LINKFRAME_TOOL_MAX_PAYLOAD + 1u];
  boomlink_tool_hex_encode(&buf[BOOMLINK_LINKFRAME_HEADER_SIZE], payload_len, hex, sizeof(hex));
  printf("payload=%s\n", hex);
  return 0;
}

static int run_accepts(int argc, char **argv) {
  if (argc != 4) {
    fprintf(stderr, "usage: accepts <destination_id> <local_node_id>\n");
    return 2;
  }
  uint32_t destination_id;
  uint32_t local_node_id;
  if (!boomlink_tool_parse_u32(argv[2], &destination_id)) {
    fprintf(stderr, "accepts: '%s' is not a valid uint32 destination_id\n", argv[2]);
    return 2;
  }
  if (!boomlink_tool_parse_u32(argv[3], &local_node_id)) {
    fprintf(stderr, "accepts: '%s' is not a valid uint32 local_node_id\n", argv[3]);
    return 2;
  }
  printf("accepts=%u\n", boomlink_linkframe_is_for_node(destination_id, local_node_id) ? 1u : 0u);
  return 0;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: %s <selftest|limits|encode|parse|accepts> [args...]\n", argv[0]);
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
  if (strcmp(argv[1], "parse") == 0) {
    return run_parse(argc, argv);
  }
  if (strcmp(argv[1], "accepts") == 0) {
    return run_accepts(argc, argv);
  }
  fprintf(stderr, "unknown subcommand '%s'\n", argv[1]);
  return 2;
}

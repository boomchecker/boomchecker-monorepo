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
 *            linkframe_tool encode     <frame_type> <flags> <fragment_index> \
 *                                      <dest> <src> <session> <sequence> \
 *                                      <payload_hex> <out_file>
 *            linkframe_tool encode_raw <same arguments>
 *            linkframe_tool parse <in_file> [expected_magic]
 *            linkframe_tool ack <in_file> <local_node_id> <out_file> \
 *                                [expected_magic]
 *            linkframe_tool accepts <destination_id> <local_node_id>
 *
 *          Both encode subcommands take a raw flags byte rather than named
 *          booleans, because the tests must be able to build frames a correct
 *          sender would never produce. They differ in one crucial way:
 *
 *          `encode` goes through boomlink_linkframe_encode() and leaves its
 *          output alone, so whatever that function decides about the flags byte
 *          is what lands in the file and is therefore observable by the tests.
 *          The requested reserved bits are handed to it via the header's
 *          reserved_flags field - which it is supposed to ignore.
 *
 *          `encode_raw` additionally overwrites the flags byte with the
 *          requested value afterwards. That is the only way to forge the "a
 *          newer sender set a bit we have never heard of" frame the reserved-bit
 *          RECEIVER rule must be tested against, since the encoder correctly
 *          refuses to emit one.
 *
 *          Keeping these separate matters: when a single `encode` did both, the
 *          override made byte 2 of the encoder's output invisible to the whole
 *          pytest suite - an encoder that set a reserved bit on every frame, or
 *          swapped the two known flags, passed 100%.
 ******************************************************************************
 */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h> /* malloc, free */
#include <string.h>

#include "boomlink_linkframe.h"
#include "tool_support.h"

/* Payload cap for this tool's buffers. The link layer itself imposes no
   payload bound - it treats the payload as opaque bytes and the radio's
   RADIO_MAX_PAYLOAD is what limits a real frame - so this is purely a test
   harness limit, sized well past the 255-byte SX126x packet ceiling so a test
   can build an oversized frame and watch it be rejected. */
#define LINKFRAME_TOOL_MAX_PAYLOAD 512u

/* The one byte offset this harness pokes at, deliberately its OWN copy of
   section 7.3's layout rather than something exported from
   boomlink_linkframe.h. A test that read the offset from the implementation
   would move with it, so an offset changing by mistake would still line up on
   both sides and prove nothing - the same reason test_linkframe.py spells out
   the expected 20 bytes as literals. */
#define LINKFRAME_TOOL_OFF_FLAGS 2u

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

  /* A truncated header must fail closed rather than read past the buffer.
     Parsed from an exactly-sized heap block, not from a prefix of `buf`: with a
     large backing object any over-read stays intra-object, where ASan is blind.
     Sized to the logical length so the redzone sits right after the last valid
     byte. */
  uint8_t *short_block = malloc(BOOMLINK_LINKFRAME_HEADER_SIZE - 1u);
  if (short_block == NULL) {
    fprintf(stderr, "selftest: out of memory\n");
    return 1;
  }
  memcpy(short_block, buf, BOOMLINK_LINKFRAME_HEADER_SIZE - 1u);
  boomlink_linkframe_header_t truncated = {0};
  size_t                     ignored   = 0u;
  boomlink_linkframe_parse_result_t short_result =
      boomlink_linkframe_parse(short_block, BOOMLINK_LINKFRAME_HEADER_SIZE - 1u,
                               BOOMLINK_LINKFRAME_MAGIC_DEFAULT, &truncated, &ignored);
  free(short_block);
  if (short_result != BOOMLINK_LINKFRAME_ERR_TOO_SHORT) {
    fprintf(stderr, "selftest: a short buffer was not rejected\n");
    return 1;
  }

  /* The encoder must drop reserved flag bits (section 7.3: "always 0 until a
     future PR assigns them"). Checked HERE, in self-contained C, as well as from
     pytest: the Python-side check depends on run_encode populating
     header.reserved_flags, and setting that field to 0 would make the pytest
     test silently vacuous again without anything turning red - which is half of
     how it was vacuous the first time. */
  boomlink_linkframe_header_t reserved_probe = {
      .magic          = BOOMLINK_LINKFRAME_MAGIC_DEFAULT,
      .version        = BOOMLINK_LINKFRAME_VERSION,
      .frame_type     = BOOMLINK_FRAME_TYPE_DATA,
      .ack_requested  = true,
      .reserved_flags = BOOMLINK_LINKFRAME_FLAGS_RESERVED_MASK,
  };
  uint8_t reserved_buf[BOOMLINK_LINKFRAME_HEADER_SIZE];
  boomlink_linkframe_encode(&reserved_probe, reserved_buf);
  if (reserved_buf[LINKFRAME_TOOL_OFF_FLAGS] != BOOMLINK_LINKFRAME_FLAG_ACK_REQUESTED) {
    fprintf(stderr,
            "selftest: the encoder emitted flags 0x%02X for a header asking for every "
            "reserved bit; it must keep only ack_requested (0x%02X)\n",
            (unsigned)reserved_buf[LINKFRAME_TOOL_OFF_FLAGS],
            (unsigned)BOOMLINK_LINKFRAME_FLAG_ACK_REQUESTED);
    return 1;
  }

  /* boomlink_linkframe_header_init() must produce a header that actually encodes
     to an ACCEPTABLE frame, and the zero-initialised header it exists to replace
     must not. Both halves matter: the first is the helper's whole purpose, and
     without the second there is nothing recording WHY it exists - an init
     function that set the same three fields to the wrong values would satisfy
     the first check alone. */
  boomlink_linkframe_header_t initialised;
  boomlink_linkframe_header_init(&initialised);
  initialised.destination_id = 0x42u;
  uint8_t initialised_buf[BOOMLINK_LINKFRAME_HEADER_SIZE];
  boomlink_linkframe_encode(&initialised, initialised_buf);
  boomlink_linkframe_header_t init_decoded  = {0};
  size_t                      init_len      = 0u;
  boomlink_linkframe_parse_result_t init_result =
      boomlink_linkframe_parse(initialised_buf, sizeof(initialised_buf),
                               BOOMLINK_LINKFRAME_MAGIC_DEFAULT, &init_decoded, &init_len);
  if (init_result != BOOMLINK_LINKFRAME_OK) {
    fprintf(stderr,
            "selftest: a header from boomlink_linkframe_header_init() did not encode to an "
            "acceptable frame: %s\n",
            boomlink_linkframe_parse_result_str(init_result));
    return 1;
  }
  if (init_decoded.destination_id != 0x42u ||
      init_decoded.frame_type != BOOMLINK_FRAME_TYPE_DATA) {
    fprintf(stderr, "selftest: an initialised header did not survive the round-trip\n");
    return 1;
  }

  boomlink_linkframe_header_t zeroed = {0};
  uint8_t zeroed_buf[BOOMLINK_LINKFRAME_HEADER_SIZE];
  boomlink_linkframe_encode(&zeroed, zeroed_buf);
  boomlink_linkframe_header_t zero_decoded = {0};
  size_t                      zero_len     = 0u;
  if (boomlink_linkframe_parse(zeroed_buf, sizeof(zeroed_buf),
                               BOOMLINK_LINKFRAME_MAGIC_DEFAULT, &zero_decoded,
                               &zero_len) == BOOMLINK_LINKFRAME_OK) {
    fprintf(stderr,
            "selftest: a zero-initialised header encoded to an ACCEPTED frame - the reason "
            "boomlink_linkframe_header_init() exists no longer holds\n");
    return 1;
  }

  /* Section 9.5's ACK mapping, checked here as well as from pytest, because the
     way it goes wrong is a transposition and this is a check that names each
     field's SOURCE explicitly rather than comparing two implementations. */
  boomlink_linkframe_header_t ack = {0};
  if (!boomlink_linkframe_make_ack(&decoded, 0x0BADF00Du, &ack)) {
    fprintf(stderr, "selftest: make_ack refused a perfectly ordinary frame\n");
    return 1;
  }
  if (ack.destination_id != header.source_id) {
    fprintf(stderr,
            "selftest: the ACK is addressed to 0x%08X; section 9.5 requires the "
            "acknowledged frame's SOURCE (0x%08X), not its destination (0x%08X)\n",
            (unsigned)ack.destination_id, (unsigned)header.source_id,
            (unsigned)header.destination_id);
    return 1;
  }
  if (ack.source_id != 0x0BADF00Du || ack.session_id != header.session_id ||
      ack.sequence != header.sequence || ack.frame_type != BOOMLINK_FRAME_TYPE_ACK ||
      ack.ack_requested || ack.more_fragments || ack.fragment_index != 0u ||
      ack.magic != header.magic || ack.version != BOOMLINK_LINKFRAME_VERSION) {
    fprintf(stderr, "selftest: the ACK header does not match section 9.5's mapping\n");
    return 1;
  }
  /* And the sender must be able to accept it - the whole point of the swap. */
  if (!boomlink_linkframe_is_for_node(ack.destination_id, header.source_id)) {
    fprintf(stderr, "selftest: the original sender would not accept its own ACK\n");
    return 1;
  }
  /* Refused when either end of the addressing is not a real node, so an ACK can
     never be aimed at the whole network. */
  boomlink_linkframe_header_t broadcast_source = decoded;
  broadcast_source.source_id                   = BOOMLINK_ADDR_BROADCAST;
  boomlink_linkframe_header_t refused = {0};
  if (boomlink_linkframe_make_ack(&broadcast_source, 0x42u, &refused) ||
      boomlink_linkframe_make_ack(&decoded, BOOMLINK_ADDR_BROADCAST, &refused) ||
      boomlink_linkframe_make_ack(&decoded, BOOMLINK_ADDR_INVALID, &refused)) {
    fprintf(stderr,
            "selftest: make_ack built an ACK with an unusable address - one aimed at the "
            "broadcast address turns a single frame into a network-wide transmission\n");
    return 1;
  }

  /* The parser's fail-safe: "on any failure *out_header is zeroed and
     *out_payload_len is set to 0, so a caller that ignores the return value
     cannot act on a partially-filled header" (see the header's contract). That
     promise is defence-in-depth aimed at the link engine, and it is checked HERE
     because it cannot be observed through the CLI at all: `parse` prints only
     result= and result_str= for a rejected frame and returns, so no pytest test
     can see the post-rejection header. Verified that without this probe,
     replacing the memset with `memset(out_header, 0xAA, ...)` and
     `*out_payload_len = 12345` leaves every test green.

     Pre-filled with 0xAA rather than left uninitialised so a parser that zeroes
     nothing is caught, and compared with memcmp against a zeroed struct rather
     than field by field so a partial memset (or a field added later and left out
     of the clear) is caught too. */
  boomlink_linkframe_header_t dirty;
  memset(&dirty, 0xAA, sizeof(dirty));
  size_t dirty_payload_len = 12345u;
  /* An all-zero buffer: rejected on magic, the earliest failure that still gets
     past the length check, so the memset is the only thing that could have
     cleared the header. */
  const uint8_t rejected_frame[BOOMLINK_LINKFRAME_HEADER_SIZE] = {0};
  boomlink_linkframe_parse_result_t dirty_result =
      boomlink_linkframe_parse(rejected_frame, sizeof(rejected_frame),
                               BOOMLINK_LINKFRAME_MAGIC_DEFAULT, &dirty,
                               &dirty_payload_len);
  if (dirty_result != BOOMLINK_LINKFRAME_ERR_MAGIC) {
    fprintf(stderr, "selftest: an all-zero frame was rejected as %s, expected a magic error\n",
            boomlink_linkframe_parse_result_str(dirty_result));
    return 1;
  }
  boomlink_linkframe_header_t all_zero;
  memset(&all_zero, 0, sizeof(all_zero));
  if (memcmp(&dirty, &all_zero, sizeof(dirty)) != 0) {
    fprintf(stderr,
            "selftest: a rejected parse left the caller's header non-zero - a caller "
            "ignoring the return value would act on stale contents\n");
    return 1;
  }
  if (dirty_payload_len != 0u) {
    fprintf(stderr,
            "selftest: a rejected parse left payload_len at %zu instead of 0\n",
            dirty_payload_len);
    return 1;
  }

  /* Deliberately NO probe for an out-of-nibble `version`, even though it is
     reachable only from here (the encode subcommands do not expose `version`,
     since every frame they build has to be a version-1 frame for the parse
     tests). Such a probe could not fail: the shift into the high nibble followed
     by the truncation to a byte discards exactly what the mask discards, for
     every input, so deleting the mask changes no output byte - verified by
     deleting it and watching the whole suite and this selftest stay green. It
     would read as coverage while being incapable of failing, which is the same
     trap as the frame_type=17 case in test_linkframe.py. The mirror-image
     frame_type mask IS observable, and is covered from pytest with 33. */

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
  printf("flags_assigned_mask=%u\n", (unsigned)BOOMLINK_LINKFRAME_FLAGS_ASSIGNED_MASK);
  printf("flags_reserved_mask=%u\n", (unsigned)BOOMLINK_LINKFRAME_FLAGS_RESERVED_MASK);
  printf("max_payload=%u\n", (unsigned)LINKFRAME_TOOL_MAX_PAYLOAD);
  /* Same reason codec_tool reports it: assert_clean_rejection()'s guarantee
     rests entirely on the sanitizers being present, so a suite running against
     an uninstrumented binary must be able to say so rather than quietly lose
     its safety net. Reported per TOOL - this one has its own negative-path
     tests (short buffers, fragmented frames, hostile flag bytes) and could in
     principle be built differently from codec_tool. */
  printf("sanitizers=%d\n", BOOMLINK_SANITIZE_ENABLED);
  /* Every parse result code, so the Python ParseResult enum is checked against
     the C enumerators rather than assumed to match. The count comes from the
     enum's own sentinel, not from counting the lines below: without it, whether
     an appended C result was noticed depended on someone remembering to add a
     printf here, and forgetting left the reverse check (C reports a result the
     Python mirror lacks) passing on an incomplete list - the exact failure the
     reverse check exists to catch. Named parse_result_count rather than
     result_count so it is not itself mistaken for one of the result_* keys. */
  printf("parse_result_count=%d\n", (int)BOOMLINK_LINKFRAME_RESULT_COUNT);
  printf("result_ok=%d\n", (int)BOOMLINK_LINKFRAME_OK);
  printf("result_too_short=%d\n", (int)BOOMLINK_LINKFRAME_ERR_TOO_SHORT);
  printf("result_magic=%d\n", (int)BOOMLINK_LINKFRAME_ERR_MAGIC);
  printf("result_version=%d\n", (int)BOOMLINK_LINKFRAME_ERR_VERSION);
  printf("result_frame_type=%d\n", (int)BOOMLINK_LINKFRAME_ERR_FRAME_TYPE);
  printf("result_fragmented=%d\n", (int)BOOMLINK_LINKFRAME_ERR_FRAGMENTED);
  printf("result_ack_has_payload=%d\n", (int)BOOMLINK_LINKFRAME_ERR_ACK_HAS_PAYLOAD);
  /* And the human-readable name of each, driven off the sentinel so every result
     is covered whether or not it was listed above. Nothing used to read these
     strings at all: swapping the ERR_MAGIC and ERR_VERSION return values in
     boomlink_linkframe_parse_result_str() left the whole suite green, so every
     wrong-magic drop in the field would have been logged as "unsupported link
     frame version". The header justifies the entire split-reason enum with
     section 9.10's requirement to count and debug these separately, which is
     worth nothing if the labels are wrong.

     Keyed parse_result_str_<n>, not result_str_<n>: the cross-check above
     collects every key starting with "result_" as a reported enumerator, so that
     spelling would have added seven phantom results to it. Same reason
     parse_result_count is spelled that way. */
  for (int result = 0; result < (int)BOOMLINK_LINKFRAME_RESULT_COUNT; result++) {
    printf("parse_result_str_%d=%s\n", result,
           boomlink_linkframe_parse_result_str((boomlink_linkframe_parse_result_t)result));
  }
  /* The catch-all, reported so the test can assert no REAL result falls through
     to it without hardcoding its wording - and so the header's "never NULL, even
     for an out-of-range value" contract has a consumer. Deliberately a value no
     enumerator has. */
  printf("parse_result_str_out_of_range=%s\n",
         boomlink_linkframe_parse_result_str(
             (boomlink_linkframe_parse_result_t)(BOOMLINK_LINKFRAME_RESULT_COUNT + 1)));
  return 0;
}

static int run_encode(int argc, char **argv, bool raw_flags) {
  if (argc != 11) {
    fprintf(stderr,
            "usage: %s <frame_type> <flags> <fragment_index> <dest> <src> <session> "
            "<sequence> <payload_hex> <out_file>\n",
            argv[1]);
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

  /* The requested reserved bits are handed to the encoder via reserved_flags,
     which it is specified to ignore. That is deliberate: it means plain
     `encode` actually ASKS the encoder for reserved bits, so a test can observe
     whether it refuses - rather than the tool answering the question itself. */
  boomlink_linkframe_header_t header = {
      .magic          = BOOMLINK_LINKFRAME_MAGIC_DEFAULT,
      .version        = BOOMLINK_LINKFRAME_VERSION,
      .frame_type     = frame_type,
      .ack_requested  = (flags & BOOMLINK_LINKFRAME_FLAG_ACK_REQUESTED) != 0u,
      .more_fragments = (flags & BOOMLINK_LINKFRAME_FLAG_MORE_FRAGMENTS) != 0u,
      .fragment_index = fragment_index,
      .reserved_flags = (uint8_t)(flags & BOOMLINK_LINKFRAME_FLAGS_RESERVED_MASK),
      .destination_id = dest,
      .source_id      = src,
      .session_id     = session,
      .sequence       = sequence,
  };

  uint8_t buf[BOOMLINK_LINKFRAME_HEADER_SIZE + LINKFRAME_TOOL_MAX_PAYLOAD];
  boomlink_linkframe_encode(&header, buf);
  if (raw_flags) {
    /* encode_raw only: forge the flags byte the encoder correctly refuses to
       emit, so the reserved-bit RECEIVER rule can be tested. Plain `encode`
       must never do this - it would make the encoder's own output unobservable,
       which is exactly how an encoder that set a reserved bit on every frame
       once passed the whole suite. */
    buf[LINKFRAME_TOOL_OFF_FLAGS] = flags;
  }
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

  /* Parse from an EXACTLY len-sized heap block, not from `buf` directly.
     Reading into a fixed 533-byte stack buffer and handing the parser the
     logical `len` means any read past `len` still lands inside a valid, much
     larger object - so it is intra-object, and AddressSanitizer cannot see it.
     Verified: a parser that read bytes 12..19 before checking the length
     accepted a 0-byte input, over-read 8 bytes, and the whole suite passed
     green. A malloc'd block of exactly `len` puts ASan's redzones immediately
     after the last valid byte, which is what makes the short-buffer tests
     mean what they claim.
     malloc(0) may legitimately return NULL, and a 0-byte input is a real test
     case (it must be rejected as too short), so a NULL block with len 0 is
     passed through rather than treated as an error. */
  uint8_t *exact = NULL;
  if (len > 0u) {
    exact = malloc(len);
    if (exact == NULL) {
      fprintf(stderr, "parse: out of memory for a %zu-byte input\n", len);
      return 1;
    }
    memcpy(exact, buf, len);
  }

  boomlink_linkframe_header_t header      = {0};
  size_t                      payload_len = 0u;
  boomlink_linkframe_parse_result_t result =
      boomlink_linkframe_parse(exact, len, expected_magic, &header, &payload_len);

  /* The result code is printed on BOTH paths, and the process exits 0 even for
     a rejected frame: a rejection is this subcommand's normal, expected output
     (the tests assert on specific reasons), not a failure of the tool. Only an
     I/O or usage problem is a nonzero exit. */
  printf("result=%d\n", (int)result);
  printf("result_str=%s\n", boomlink_linkframe_parse_result_str(result));
  if (result != BOOMLINK_LINKFRAME_OK) {
    free(exact);
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
  boomlink_tool_hex_encode(&exact[BOOMLINK_LINKFRAME_HEADER_SIZE], payload_len, hex, sizeof(hex));
  printf("payload=%s\n", hex);
  free(exact);
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

/* Read a frame from `path` into `buf` (capacity `cap`), returning its length or
   -1 with a message already printed. Shared by `parse` and `ack`. */
static long read_frame_file(const char *subcommand, const char *path, uint8_t *buf,
                           size_t cap) {
  FILE *f = fopen(path, "rb");
  if (f == NULL) {
    fprintf(stderr, "%s: could not open '%s' for reading\n", subcommand, path);
    return -1;
  }
  size_t len  = fread(buf, 1, cap, f);
  int    ferr = ferror(f);
  fclose(f);
  if (ferr) {
    fprintf(stderr, "%s: read error on '%s'\n", subcommand, path);
    return -1;
  }
  return (long)len;
}

static int run_ack(int argc, char **argv) {
  if (argc != 5 && argc != 6) {
    fprintf(stderr, "usage: ack <in_file> <local_node_id> <out_file> [expected_magic]\n");
    return 2;
  }
  const char *in_path  = argv[2];
  const char *out_path = argv[4];
  uint32_t    local_node_id;
  if (!boomlink_tool_parse_u32(argv[3], &local_node_id)) {
    fprintf(stderr, "ack: '%s' is not a valid uint32 local_node_id\n", argv[3]);
    return 2;
  }
  uint8_t expected_magic = BOOMLINK_LINKFRAME_MAGIC_DEFAULT;
  if (argc == 6 && !boomlink_tool_parse_u8(argv[5], &expected_magic)) {
    fprintf(stderr, "ack: '%s' is not a valid uint8 expected_magic\n", argv[5]);
    return 2;
  }

  uint8_t buf[BOOMLINK_LINKFRAME_HEADER_SIZE + LINKFRAME_TOOL_MAX_PAYLOAD];
  long    len = read_frame_file("ack", in_path, buf, sizeof(buf));
  if (len < 0) {
    return 1;
  }

  boomlink_linkframe_header_t received    = {0};
  size_t                      payload_len = 0u;
  boomlink_linkframe_parse_result_t result =
      boomlink_linkframe_parse(buf, (size_t)len, expected_magic, &received, &payload_len);
  if (result != BOOMLINK_LINKFRAME_OK) {
    /* This subcommand acknowledges an ACCEPTED frame, so an unparseable input is
       a harness mistake rather than a result to report - `parse` is where a
       rejection is the expected output. */
    fprintf(stderr, "ack: '%s' does not parse (%s); nothing to acknowledge\n", in_path,
            boomlink_linkframe_parse_result_str(result));
    return 1;
  }

  boomlink_linkframe_header_t ack = {0};
  /* Printed rather than turned into an exit code, the same way `accepts` reports
     its answer: a refusal is a legitimate result of this function, not an error,
     and keeping it on stdout means a test cannot confuse it with an I/O failure
     or a sanitizer abort. */
  const bool built = boomlink_linkframe_make_ack(&received, local_node_id, &ack);
  printf("ack_built=%u\n", built ? 1u : 0u);
  if (!built) {
    return 0;
  }

  uint8_t ack_buf[BOOMLINK_LINKFRAME_HEADER_SIZE];
  boomlink_linkframe_encode(&ack, ack_buf);
  FILE *out = fopen(out_path, "wb");
  if (out == NULL) {
    fprintf(stderr, "ack: could not open '%s' for writing\n", out_path);
    return 1;
  }
  size_t written = fwrite(ack_buf, 1, sizeof(ack_buf), out);
  fclose(out);
  if (written != sizeof(ack_buf)) {
    fprintf(stderr, "ack: short write to '%s'\n", out_path);
    return 1;
  }
  return 0;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr,
            "usage: %s <selftest|limits|encode|encode_raw|parse|ack|accepts> [args...]\n",
            argv[0]);
    return 2;
  }
  if (strcmp(argv[1], "selftest") == 0) {
    return run_selftest();
  }
  if (strcmp(argv[1], "limits") == 0) {
    return run_limits();
  }
  if (strcmp(argv[1], "encode") == 0) {
    return run_encode(argc, argv, false);
  }
  if (strcmp(argv[1], "encode_raw") == 0) {
    return run_encode(argc, argv, true);
  }
  if (strcmp(argv[1], "parse") == 0) {
    return run_parse(argc, argv);
  }
  if (strcmp(argv[1], "ack") == 0) {
    return run_ack(argc, argv);
  }
  if (strcmp(argv[1], "accepts") == 0) {
    return run_accepts(argc, argv);
  }
  fprintf(stderr, "unknown subcommand '%s'\n", argv[1]);
  return 2;
}

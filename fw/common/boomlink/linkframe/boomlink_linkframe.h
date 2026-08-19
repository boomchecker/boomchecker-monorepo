/**
 ******************************************************************************
 * @file    boomlink_linkframe.h
 * @brief   BoomLink's fixed 20-byte binary link frame header (boomlink.md
 *          section 7.3): encode, parse and validate.
 *
 *          Deliberately has NO Nanopb dependency, and must never gain one.
 *          boomlink.md section 9 is explicit: "BoomLink never decodes the
 *          Protobuf payload and has no Nanopb dependency" - the link layer has
 *          to be able to filter, acknowledge and deduplicate a packet, and
 *          reject foreign traffic from a few leading bytes, without invoking a
 *          Protobuf decoder. To this module the payload is opaque bytes: it
 *          reports where the payload starts and how long it is, and nothing
 *          more. The CMake target enforces the boundary by not linking Nanopb.
 *
 *          Target-agnostic: no STM32/HAL dependency, no radio dependency,
 *          no global state. Every function here is pure.
 *
 *          C++ callers: include this header BARE, never wrapped in
 *          `extern "C" { }`. It manages its own linkage, and it ends in a
 *          template (see the bottom of the file) - a template cannot have C
 *          language linkage, so wrapping the include is a hard error, "template
 *          with C linkage". Said here because the firmware's C++ radio layer,
 *          the caller that template exists for, wraps its C includes exactly
 *          that way today. tests/encoder_bound_ok.cpp pins the bare form.
 ******************************************************************************
 */
#ifndef BOOMLINK_LINKFRAME_H
#define BOOMLINK_LINKFRAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* boomlink.md section 7.3's layout, little-endian:
     offset  size  field
     0       1     magic / network ID
     1       1     version (high nibble) | frame type (low nibble)
     2       1     flags   (bit 0: ack_requested, bit 1: more_fragments)
     3       1     fragment_index
     4       4     destination_id
     8       4     source_id
     12      4     session_id
     16      4     sequence
   Must stay equal to BOOMLINK_LINK_FRAME_HEADER_SIZE in boomlink_codec.h,
   which the codec uses to compute the on-air Envelope budget. The two are
   deliberately separate definitions in separate layers (the codec must not
   depend on this header, nor this on the codec); a _Static_assert in the
   tests pins them together so they cannot drift. */
#define BOOMLINK_LINKFRAME_HEADER_SIZE 20u

/* Used to declare a single-object output parameter as `T p[static 1]`. Worth
   exactly what it says and no more: it puts "must not be NULL" in the signature
   instead of only in a doc comment, and GCC/clang diagnose a LITERAL null passed
   there (-Wnonnull). A null that arrives through a variable is not caught - the
   sanitizer build is what covers that. `[static N]` is C only, so under C++ it
   degrades to a plain bound, which decays to a pointer exactly as before. */
#if defined(__cplusplus)
#define BOOMLINK_LINKFRAME_ONE 1
#else
#define BOOMLINK_LINKFRAME_ONE static 1
#endif

/* Default magic / network ID. Runtime-configurable per section 7.3 so two
   deployments in radio range can ignore each other's traffic cheaply; the
   parse function takes the expected value as an argument rather than reading
   a global, both to stay pure and so a promiscuous/debug mode can accept
   another network without changing normal behaviour. */
#define BOOMLINK_LINKFRAME_MAGIC_DEFAULT 0xB0u

/* Version occupies the high nibble of byte 1, so it must fit in 4 bits.
   Section 7.3: any future change to how the payload itself must be framed or
   interpreted has to be gated behind this nibble rather than added as a bare
   flag bit, so that an old receiver's "ignore what you don't know" default can
   never be the unsafe choice. */
#define BOOMLINK_LINKFRAME_VERSION 1u

/* Address space, section 7.2. */
#define BOOMLINK_ADDR_INVALID   0x00000000u
#define BOOMLINK_ADDR_BROADCAST 0xFFFFFFFFu

/* flags bit assignments, section 7.3. Bits 2-7 are reserved and always sent
   as 0; a receiver must IGNORE an unrecognized one rather than drop the frame
   (ordinary forward compatibility - see the parse function's contract).
   ASSIGNED_MASK is the list of bits this build understands, and RESERVED_MASK is
   DERIVED from it rather than written out as 0xFC: the two can then never
   disagree, which they otherwise could, and the parser would report an
   already-assigned bit as "reserved/unrecognized" - a wrong answer in the one
   field whose entire purpose is to say what a newer peer sent that we do not
   understand. Assigning a future bit therefore means adding it to ASSIGNED_MASK
   here (and to the Python mirror, which the test suite pins to these values). */
#define BOOMLINK_LINKFRAME_FLAG_ACK_REQUESTED  0x01u
#define BOOMLINK_LINKFRAME_FLAG_MORE_FRAGMENTS 0x02u
#define BOOMLINK_LINKFRAME_FLAGS_ASSIGNED_MASK \
  ((uint8_t)(BOOMLINK_LINKFRAME_FLAG_ACK_REQUESTED | BOOMLINK_LINKFRAME_FLAG_MORE_FRAGMENTS))
#define BOOMLINK_LINKFRAME_FLAGS_RESERVED_MASK \
  ((uint8_t)(0xFFu & ~(unsigned)BOOMLINK_LINKFRAME_FLAGS_ASSIGNED_MASK))

typedef enum {
  /* Wire values from section 7.3. 0 is deliberately not a valid frame type,
     so a zeroed buffer never parses as a usable frame. */
  BOOMLINK_FRAME_TYPE_DATA = 1,
  BOOMLINK_FRAME_TYPE_ACK  = 2,
} boomlink_frame_type_t;

/* Section 9.5's SENDER rules - "ACK packets never request another ACK",
   "broadcast packets never request ACK" - are deliberately NOT enforced in this
   module and are not bugs by their absence. They are properties of the TX
   pipeline (section 9.1), which is the link engine's job: this layer is a
   stateless codec for one header, and a parser must report what actually
   arrived rather than what a compliant sender would have sent. Whoever builds
   the engine owns them. Said out loud because nothing else in this phase
   mentions them, and their absence here should not read as an oversight. */

typedef struct {
  uint8_t  magic;
  uint8_t  version;
  uint8_t  frame_type;      /* boomlink_frame_type_t */
  bool     ack_requested;
  bool     more_fragments;
  uint8_t  fragment_index;
  /* The reserved flags bits 2-7 exactly as received, so a caller that wants
     to log or diagnose unexpected traffic can see them. Parsing does not
     reject a frame for these being set (section 7.3), and an encoder always
     sends them as 0 - this field is ignored by boomlink_linkframe_encode(). */
  uint8_t  reserved_flags;
  uint32_t destination_id;
  uint32_t source_id;
  uint32_t session_id;
  uint32_t sequence;
} boomlink_linkframe_header_t;

/**
 * Why a parse attempt failed. Separate reasons rather than one boolean because
 * boomlink.md section 9.10 requires several of these to be counted
 * independently ("malformed packets", "packets ignored for another
 * destination", "packets rejected by magic/network ID or version"), and
 * because a link that cannot say WHY it is dropping traffic is very hard to
 * debug in the field.
 */
typedef enum {
  BOOMLINK_LINKFRAME_OK = 0,
  /* Fewer bytes than a header. */
  BOOMLINK_LINKFRAME_ERR_TOO_SHORT,
  /* Foreign network, or a version this build does not implement. Section 7.3
     requires these to be dropped and counted "before any further
     processing". */
  BOOMLINK_LINKFRAME_ERR_MAGIC,
  BOOMLINK_LINKFRAME_ERR_VERSION,
  /* Neither DATA nor ACK. */
  BOOMLINK_LINKFRAME_ERR_FRAME_TYPE,
  /* more_fragments set, or a non-zero fragment_index. Section 7.3 requires
     dropping on EITHER: a fragmented message's last fragment correctly has
     more_fragments = 0 while fragment_index is non-zero, and a receiver that
     checked only more_fragments would hand that tail to the decoder as if it
     were a whole Envelope. */
  BOOMLINK_LINKFRAME_ERR_FRAGMENTED,
  /* An ACK frame carrying a payload. Section 9.5: "An ACK frame has no
     payload". */
  BOOMLINK_LINKFRAME_ERR_ACK_HAS_PAYLOAD,
  /* Not a result: how many there are. Keep last, and keep every real result
     above it contiguous from 0. It exists so the test tool can report the COUNT
     without hand-maintaining a number: the cross-language test checks that the
     Python mirror of this enum has exactly this many members AND that every
     name/value pair matches, so appending a result here without also reporting
     it (tests/linkframe_tool.c's `limits`) and mirroring it in Python fails
     instead of silently giving the reference parser a drop reason it can never
     produce. */
  BOOMLINK_LINKFRAME_RESULT_COUNT,
} boomlink_linkframe_parse_result_t;

/**
 * Fill `header` with the defaults every outgoing frame needs: this build's magic
 * and version, and `frame_type` DATA. Every other field is zeroed, so the caller
 * sets only what it actually means to say.
 *
 * Exists because `boomlink_linkframe_header_t h = {0}` is a trap and the encoder
 * cannot refuse it: magic 0, version 0 and frame type 0 are each invalid, so a
 * zero-initialised header encodes to 20 bytes no receiver will ever accept - and
 * the encoder has no failure path to say so (by design; see below). Nothing
 * about the struct hints that three of its fields must not be left at their
 * natural default.
 *
 * This also brings the C to parity with the Python reference, whose
 * LinkFrameHeader dataclass has carried these three as field defaults from the
 * start - so the two implementations were not equally easy to misuse.
 *
 * Not a constructor and not required: a caller that sets all three itself is
 * perfectly correct, and the parser fills a header without going through here.
 */
void boomlink_linkframe_header_init(
    boomlink_linkframe_header_t out_header[BOOMLINK_LINKFRAME_ONE]);

/**
 * Serialize `header` into `out`, which must have room for
 * BOOMLINK_LINKFRAME_HEADER_SIZE bytes - and the compiler enforces that, in
 * both languages (see below). Writes byte by byte in explicit little-endian
 * order rather than copying a struct, so the wire format does not depend on the
 * host's endianness, alignment or padding.
 *
 * Takes no size argument and cannot fail, unlike
 * boomlink_encode_envelope(): the output length is a compile-time constant, so
 * a capacity parameter would only move a guaranteed-satisfiable check to
 * runtime. The array bound is what makes that safe.
 *
 * `header->reserved_flags` is ignored and the reserved bits are written as 0
 * (section 7.3: always 0 until a future PR assigns them). `frame_type` is masked
 * to its nibble, which is load-bearing - the low nibble is OR'd under the
 * version nibble, so an out-of-range type would otherwise corrupt the version.
 * `version` is masked too, but that mask cannot change the emitted byte: the
 * shift into the high nibble discards the same bits the mask would (see the
 * encoder), so it is there to state the intent, not as a guard.
 *
 * Declared for C only. C++ callers get the same function, and the same bound,
 * through the template at the bottom of this header - see the comment there.
 */
#if !defined(__cplusplus)
/* `uint8_t out[static 20]` is not decoration: it tells the compiler the callee
   always writes 20 bytes, which makes GCC reject an undersized caller buffer at
   COMPILE time (-Wstringop-overflow, an error under this package's -Werror).
   Without it the encoder writes 20 bytes unconditionally, and the dangerous case
   is not the obvious one - a too-small separate object is caught by
   AddressSanitizer, but writing past a short array INSIDE a larger struct
   silently corrupts the neighbouring member with no ASan report and no warning
   at all. That is the same intra-object class that cost this package a round of
   review on the Protobuf side. Verified to fire at every optimization level,
   -O0 included, on host GCC/clang and on arm-none-eabi-gcc. */
void boomlink_linkframe_encode(const boomlink_linkframe_header_t *header,
                               uint8_t out[static BOOMLINK_LINKFRAME_HEADER_SIZE]);
#endif

/**
 * Parse and validate the link frame header at the start of `buf` (`len`
 * bytes), rejecting anything whose magic does not equal `expected_magic`.
 *
 * On BOOMLINK_LINKFRAME_OK, fills `*out_header`, and sets `*out_payload_len`
 * to the number of payload bytes following the header (0 for an ACK) - the
 * payload itself starts at `buf + BOOMLINK_LINKFRAME_HEADER_SIZE` and is NOT
 * interpreted here. On any failure `*out_header` is zeroed and
 * `*out_payload_len` is set to 0, so a caller that ignores the return value
 * cannot act on a partially-filled header.
 *
 * Deliberately does NOT check the destination: address acceptance is a
 * property of the receiving node (section 7.2), not of the frame, and keeping
 * it out means this function stays pure and a promiscuous monitoring mode
 * needs no separate parser. Use boomlink_linkframe_is_for_node().
 *
 * A DATA frame with a zero-length payload parses successfully. That is the
 * layering in section 9.2: a bad link header is a BoomLink-layer failure, a
 * payload that is not a valid Envelope is a BoomProtocol-layer failure, and
 * the two are counted separately - so this function must not pre-empt the
 * codec's judgement about the payload.
 *
 * No MAXIMUM length is enforced, and `*out_payload_len` is therefore
 * unbounded. Section 9.2's RX pipeline lists "validate magic/version + frame
 * length"; the minimum is checked here, but the ceiling is the radio's
 * (RADIO_MAX_PAYLOAD), and this module deliberately has no radio dependency,
 * so it cannot know it. Section 7.3 puts oversize rejection on the TX side
 * ("An oversized frame is rejected before transmission"). The caller owns the
 * radio budget - stated explicitly because this function does perform two
 * other length checks, so a reader could reasonably assume it performs this
 * one too.
 */
boomlink_linkframe_parse_result_t boomlink_linkframe_parse(
    const uint8_t *buf, size_t len, uint8_t expected_magic,
    boomlink_linkframe_header_t out_header[BOOMLINK_LINKFRAME_ONE],
    size_t out_payload_len[BOOMLINK_LINKFRAME_ONE]);

/**
 * Whether a node whose address is `local_node_id` should accept a frame
 * addressed to `destination_id` - section 7.2's rule: the destination matches
 * the node exactly, or is the broadcast address.
 *
 * A node whose own address is not a valid node ID accepts nothing at all,
 * broadcast included. Section 7.2 puts real node IDs at 0x00000001..0xFFFFFFFE,
 * so that means both ends of the range: BOOMLINK_ADDR_INVALID (unconfigured) and
 * BOOMLINK_ADDR_BROADCAST (a misconfiguration - the broadcast address is not
 * something a node can BE). Acting on traffic before knowing who you are is how
 * a half-provisioned node ends up answering for someone else, and a node that
 * thinks it is the broadcast address would answer for everyone.
 */
bool boomlink_linkframe_is_for_node(uint32_t destination_id, uint32_t local_node_id);

/** Human-readable name for a parse result, for CLI/diagnostics and test
 *  failure messages. Never NULL, even for an out-of-range value. */
const char *boomlink_linkframe_parse_result_str(boomlink_linkframe_parse_result_t result);

#ifdef __cplusplus
}  /* extern "C" */

/* The encoder's output bound, restored for C++ callers.
 *
 * This matters because the caller that will actually encode frames is C++: the
 * radio layer is C++ (fw/bom-stm32node/App/radio/, which exists for RadioLib),
 * so the one place the intra-object corruption above is reachable is the one
 * place `[static N]` - a C-only construct - would have said nothing at all.
 *
 * Shape: the pointer-taking prototype is declared ONLY inside this namespace,
 * so the name a C++ caller finds at global scope is the template below, which
 * binds a reference to the caller's array and checks its size. The C function
 * would otherwise win overload resolution outright - array-to-pointer decay is
 * an lvalue transformation, which [over.ics.rank] excludes when ranking, leaving
 * a tie that the "prefer a non-template" tiebreaker settles against the
 * template. Verified: with both at global scope the static_assert below is never
 * even instantiated, so a 4-byte buffer compiles clean.
 *
 * `extern "C"` inside a namespace still refers to the unmangled C symbol
 * (namespaces do not participate in C language linkage), so this is a
 * compile-time-only construct: the emitted call is to boomlink_linkframe_encode
 * exactly as from C. Verified with nm on all three compilers.
 *
 * Stricter than the C side on purpose: a C++ caller holding a bare `uint8_t *`
 * (a pointer INTO a larger buffer, say) does not match the template and must
 * call boomlink_linkframe_detail::boomlink_linkframe_encode() explicitly, which
 * is a deliberate speed bump - that is precisely the call whose bounds nothing
 * can check, so it should be visible at the call site.
 */
namespace boomlink_linkframe_detail {
extern "C" void boomlink_linkframe_encode(const boomlink_linkframe_header_t *header,
                                          uint8_t *out);
}  // namespace boomlink_linkframe_detail

template <size_t N>
inline void boomlink_linkframe_encode(const boomlink_linkframe_header_t *header,
                                      uint8_t (&out)[N]) {
  static_assert(N >= BOOMLINK_LINKFRAME_HEADER_SIZE,
                "the output buffer is smaller than a BoomLink link frame header");
  boomlink_linkframe_detail::boomlink_linkframe_encode(header, &out[0]);
}
#endif /* __cplusplus */

#endif /* BOOMLINK_LINKFRAME_H */

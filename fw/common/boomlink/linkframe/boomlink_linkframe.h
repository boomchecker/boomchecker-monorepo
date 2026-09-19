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

/* For a return value that must not be dropped. Unlike boomlink_linkframe_parse()
   - whose caller sees a zeroed header either way and so merely gets nothing -
   ignoring boomlink_linkframe_make_ack()'s answer means encoding a zeroed header
   and putting 20 bytes of magic 0 / version 0 / type 0 on the air, which costs
   airtime under section 6.1's duty-cycle budget and cannot be acted on by any
   receiver. C23 has [[nodiscard]]; this package is C11, so the attribute is
   used where it exists and disappears elsewhere. */
#if defined(__GNUC__) || defined(__clang__)
#define BOOMLINK_LINKFRAME_MUST_CHECK __attribute__((warn_unused_result))
#else
#define BOOMLINK_LINKFRAME_MUST_CHECK
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

/* Section 9.5 is split between this module and the link engine, along the same
   line as everything else here: the stateless part is here, the stateful part is
   not.
     * The ACK frame's field MAPPING is here - see
       boomlink_linkframe_make_ack(). It is a pure function of one received
       header, so it belongs to the header codec, and this is the layer whose
       tests can catch a transposed field (see that function's comment).
     * The SENDER rules are NOT, and are not bugs by their absence: "broadcast
       packets never request ACK" and "receiving a duplicate of an ACK-requested
       packet causes the ACK to be resent" are properties of the TX pipeline
       (section 9.1) and of duplicate state, which is the engine's job. A parser
       must also report what actually arrived rather than what a compliant sender
       would have sent, which is why parse() does not enforce them either.
     * Two more engine responsibilities that section 9.5 does not spell out, both
       of which this layer structurally cannot take on: do not acknowledge a frame
       addressed to the broadcast address (that is the actual ACK-storm vector,
       and make_ack() will happily build the ACK for one), and do not acknowledge
       a frame accepted in promiscuous mode (see make_ack() on the echoed magic -
       the ACK would land on a foreign network).
   The one sender rule that IS satisfied here is "ACK packets never request
   another ACK", because make_ack() clears the flag by construction rather than
   by remembering to. Said out loud because nothing else in this phase mentions
   these rules, and their absence should not read as an oversight. */

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
 * Build the ACK header that acknowledges `received`, as section 9.5 specifies:
 *
 *     frame type      = ACK
 *     destination_id  = the received frame's SOURCE
 *     source_id       = local_node_id (the acknowledging node)
 *     session_id      = the received frame's session_id
 *     sequence        = the received frame's sequence
 *     ack_requested   = 0
 *
 * @return false, leaving `*out_ack` zeroed, if the ACK could not be addressed:
 *         `received->source_id` or `local_node_id` is not a valid unicast node
 *         ID (section 7.2 puts those at 0x00000001..0xFFFFFFFE). See below.
 *         The two refusals are not distinguished, which section 9.10's counter
 *         list does not require - but note a frame that reaches this refusal has
 *         passed parse() and is_for_node(), so it is counted by neither the
 *         malformed nor the wrong-destination statistic. An engine that wants it
 *         counted has to do so at the call site.
 *
 * `received` and `out_ack` may be the SAME object: every value needed is read
 * before `*out_ack` is touched.
 *
 * Lives in this layer, rather than in the engine that will call it, because it
 * is a pure function of one header - and because THIS is the layer whose tests
 * can catch the way it goes wrong. Every field above is either copied or moved,
 * four of them are uint32_t, and getting one wrong produces a perfectly
 * well-formed ACK frame: right magic, right version, right type, parses clean,
 * addressed to the wrong node or carrying a transposed (session, sequence). On
 * air that reads as "the link does not work" - ACK timeout, retry, retry, TX
 * failure - which is diagnosed as an RF or timing problem, not as a field swap.
 *
 * The engine's own fake-radio tests cannot be relied on to catch it, and that is
 * the point: if the engine both BUILDS the ACK and MATCHES it, the same
 * misreading applied to both sides cancels out. Transpose session and sequence
 * when building and again when matching, and every delivery test passes while
 * the frames on the air violate the spec and nothing interoperates. Here, the
 * independent Python reference and the byte-exact vector pinned to section 9.5's
 * text have no such loop to hide in.
 *
 * `magic` is echoed from `received` rather than defaulted, because defaulting is
 * worse: a deployment on a non-default network ID would have its ACKs dropped by
 * the sender on the magic check, producing exactly the ACK-timeout-read-as-RF
 * failure described above. But echoing is NOT automatically safe, and the
 * tempting justification - "the frame was accepted, so its magic is ours" - is
 * false. parse() takes `expected_magic` as a parameter specifically so a
 * promiscuous mode can accept another network (section 10's
 * `promiscuous_monitor_enabled`), so a node in that mode echoing the magic back
 * would inject traffic into a deployment it is not a member of - the one thing
 * the network ID exists to prevent. Not acknowledging a frame accepted
 * promiscuously is therefore an engine responsibility, listed with the others
 * above; this function cannot tell the two cases apart, having no configuration
 * to compare against.
 *
 * `version` is this build's constant rather than the received value. For any
 * header that came from parse() the two are necessarily equal, since parse()
 * rejects every other version - but that reasoning covers only that path, and a
 * caller that fills a header itself (or uses header_init() and then sets the
 * field) has its version silently replaced. Deliberate: this build emits what it
 * implements. Worth flagging that no test at this layer can defend the choice,
 * since no input the suite can construct carries another version.
 *
 * Deliberately does NOT decide WHETHER to send an ACK. "Was one requested",
 * "is this a broadcast", "is this a duplicate whose ACK must be resent", "was
 * this frame accepted promiscuously" are all engine questions (see the section
 * 9.5 note above). The one check it does make is about the ACK frame itself: by
 * section 7.2 the broadcast and unconfigured addresses are not something a node
 * can BE, so an ACK claiming either end is not a valid ACK. Such an ACK is also
 * unusable by anyone - section 9.5's matching rule requires an ACK's destination
 * to equal the receiving node's own ID, which the broadcast address never does,
 * so a compliant matcher discards it. Frame validity, not policy, which is why
 * the check is here.
 *   An earlier version of this comment claimed a broadcast-addressed ACK could
 *   instead SATISFY an unrelated pending ACK wait sharing a (session_id,
 *   sequence). That is wrong for the reason just given, and only an
 *   over-permissive matcher - one ignoring the addressing entirely - could be
 *   fooled that way. Noted because the claim reached five separate places before
 *   it was caught.
 *
 * Note what this is NOT: it is not a defence against an ACK storm. A
 * broadcast-addressed ACK is one 20-byte transmission, exactly the airtime of a
 * unicast one, so there is no amplification in it. The N:1 vector is a frame
 * addressed to the broadcast address WITH ack_requested set, which every node in
 * range would answer - and this function builds that ACK without complaint,
 * because its source is an ordinary unicast node. Suppressing it is the engine's
 * job ("broadcast packets never request ACK", and PR 3's "broadcast causes no ACK
 * storm"). Said explicitly so this guard is not mistaken for covering it.
 */
BOOMLINK_LINKFRAME_MUST_CHECK
bool boomlink_linkframe_make_ack(const boomlink_linkframe_header_t received[BOOMLINK_LINKFRAME_ONE],
                                 uint32_t local_node_id,
                                 boomlink_linkframe_header_t out_ack[BOOMLINK_LINKFRAME_ONE]);

/**
 * Whether `ack` acknowledges `pending` for a node whose address is
 * `local_node_id` - the inverse of boomlink_linkframe_make_ack()'s mapping, and
 * section 9.2's "match ACK frames against the pending TX".
 *
 * All five conditions must hold: `ack` is an ACK frame, its `session_id` and
 * `sequence` equal the awaited frame's, its `source_id` is the node the awaited
 * frame was addressed to, and its `destination_id` is this node.
 *
 * Lives here rather than in the link engine for one reason, and it is not the
 * one that put the builder here. A matcher that TRANSPOSES a pair cannot hide:
 * it fails to match ACKs from the pinned builder, and the engine's first
 * delivery test catches it. The error that hides is the opposite one - an
 * OVER-PERMISSIVE matcher. One comparing only (session_id, sequence), ignoring
 * the addressing entirely, accepts another node's ACK for its own traffic, or a
 * broadcast-addressed ACK. Every delivery test still passes, because a correct
 * ACK matches too; what breaks is only REJECTION, which no delivery test
 * exercises. Pinning it here, against explicit near-miss vectors, is what makes
 * that visible.
 *
 * Not checked here, deliberately: `magic` and `version`, which parse() has
 * already enforced on anything that reached this point, and the stop-and-wait
 * rule itself (whether a frame is pending at all) which is engine state.
 *
 * Both `local_node_id` and `pending->destination_id` must be real node IDs, and
 * neither check is a formality. A broadcast `pending` frame should never be in
 * the ACK-pending slot at all - broadcast is not acknowledged (section 9.9) - but
 * if it is, an ACK forged with `source_id` = 0xFFFFFFFF satisfies every field
 * comparison and would match. make_ack() cannot build that ACK, which is
 * precisely why trusting that nobody sends it would be wrong. Symmetrically, an
 * ACK addressed to 0xFFFFFFFF would match at a node misconfigured to that
 * address.
 */
BOOMLINK_LINKFRAME_MUST_CHECK
bool boomlink_linkframe_ack_matches(
    const boomlink_linkframe_header_t pending[BOOMLINK_LINKFRAME_ONE],
    const boomlink_linkframe_header_t ack[BOOMLINK_LINKFRAME_ONE],
    uint32_t local_node_id);

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

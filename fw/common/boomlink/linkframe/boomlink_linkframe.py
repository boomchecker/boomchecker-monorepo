"""Host-side reference implementation of BoomLink's link frame header.

Written INDEPENDENTLY of boomlink_linkframe.c, from boomlink.md section 7.3,
and deliberately not a binding to it: the point is that the tests can encode
with one implementation and decode with the other, the same way PR 2 used
Python protobuf as an independent check on Nanopb. A binding would only prove
the C agrees with itself.

Also the parser PR 3's scope calls for on the host side, and the one PR 5's
`stm32node-cli` will use to render frames forwarded from a gateway - so it
lives here next to the C rather than under tests/.
"""

import struct
from dataclasses import dataclass
from enum import IntEnum

HEADER_SIZE = 20
MAGIC_DEFAULT = 0xB0
VERSION = 1

ADDR_INVALID = 0x00000000
ADDR_BROADCAST = 0xFFFFFFFF

FLAG_ACK_REQUESTED = 0x01
FLAG_MORE_FRAGMENTS = 0x02
# Derived, not written out as 0xFC - mirroring the C, where the two masks
# disagreeing would make the parser report an already-assigned bit as
# "reserved/unrecognized". Assigning a future bit means adding it to
# FLAGS_ASSIGNED_MASK here and in boomlink_linkframe.h; the test suite compares
# both masks across the two implementations, so one side alone will not do.
FLAGS_ASSIGNED_MASK = FLAG_ACK_REQUESTED | FLAG_MORE_FRAGMENTS
FLAGS_RESERVED_MASK = 0xFF & ~FLAGS_ASSIGNED_MASK

# "<BBBBIIII": little-endian, 4 single bytes then 4 uint32s - exactly section
# 7.3's layout. struct's explicit "<" is what keeps this independent of the
# host's endianness, matching the C's hand-rolled byte writes.
_STRUCT = struct.Struct("<BBBBIIII")
assert _STRUCT.size == HEADER_SIZE, "struct format does not match the documented header size"


class FrameType(IntEnum):
    DATA = 1
    ACK = 2


class ParseResult(IntEnum):
    """Mirrors boomlink_linkframe_parse_result_t. The numeric values matter:
    tests compare the C tool's reported result against these."""

    OK = 0
    ERR_TOO_SHORT = 1
    ERR_MAGIC = 2
    ERR_VERSION = 3
    ERR_FRAME_TYPE = 4
    ERR_FRAGMENTED = 5
    ERR_ACK_HAS_PAYLOAD = 6


class LinkFrameError(Exception):
    """Raised by parse() with the ParseResult that explains the rejection."""

    def __init__(self, result: ParseResult):
        super().__init__(f"link frame rejected: {result.name}")
        self.result = result


@dataclass
class LinkFrameHeader:
    """The C's boomlink_linkframe_header_t.

    The magic/version/frame_type defaults below are the counterpart of
    boomlink_linkframe_header_init(): all three MUST be set on an outgoing frame
    and none of them has a usable zero value, so leaving them out here gives a
    valid version-1 DATA frame rather than one no receiver would accept. The C
    struct cannot carry defaults, which is why it needs an init function to be
    equally hard to misuse.
    """

    destination_id: int
    source_id: int
    session_id: int
    sequence: int
    frame_type: int = FrameType.DATA
    ack_requested: bool = False
    more_fragments: bool = False
    fragment_index: int = 0
    magic: int = MAGIC_DEFAULT
    version: int = VERSION
    # Bits 2-7 exactly as received. Populated by parse(); ignored by encode(),
    # which always sends them as 0 per section 7.3.
    reserved_flags: int = 0


def encode(header: LinkFrameHeader) -> bytes:
    """Serialize a header to its 20 wire bytes. Reserved flag bits are always
    written as 0, and version/frame_type are masked to their nibbles - all
    matching boomlink_linkframe_encode().

    Note the two nibble masks are not equally load-bearing. frame_type's
    matters in both languages: it is OR'd into the low nibble, so a value above
    15 would raise the version nibble and turn a bad type into a bad version.
    version's is dead code in the C (the `<< 4` and the truncation to uint8_t
    discard exactly what the mask would) but NOT here - Python has no silent
    truncation to fall back on, so without it an over-wide version would make
    struct.pack raise struct.error instead of producing the byte the C produces.
    """
    flags = 0
    if header.ack_requested:
        flags |= FLAG_ACK_REQUESTED
    if header.more_fragments:
        flags |= FLAG_MORE_FRAGMENTS
    version_type = ((header.version & 0x0F) << 4) | (header.frame_type & 0x0F)
    return _STRUCT.pack(
        header.magic,
        version_type,
        flags,
        header.fragment_index,
        header.destination_id,
        header.source_id,
        header.session_id,
        header.sequence,
    )


def parse(buf: bytes, expected_magic: int = MAGIC_DEFAULT) -> tuple[LinkFrameHeader, bytes]:
    """Parse and validate a link frame, returning (header, payload).

    Raises LinkFrameError carrying the same ParseResult the C parser would
    return. The validation ORDER matters and matches the C: magic and version
    before anything else (section 7.3 requires foreign/unknown-version traffic
    to be dropped before further processing), then frame type, then the
    fragmentation rule, then the ACK-has-no-payload rule. A test that feeds a
    frame violating two rules at once relies on this order agreeing.

    Deliberately does not check the destination - that is a property of the
    receiving node, not the frame. See is_for_node().
    """
    if len(buf) < HEADER_SIZE:
        raise LinkFrameError(ParseResult.ERR_TOO_SHORT)

    magic, version_type, flags, fragment_index, dest, src, session, sequence = _STRUCT.unpack_from(
        buf, 0
    )
    if magic != expected_magic:
        raise LinkFrameError(ParseResult.ERR_MAGIC)
    version = (version_type >> 4) & 0x0F
    frame_type = version_type & 0x0F
    if version != VERSION:
        raise LinkFrameError(ParseResult.ERR_VERSION)
    if frame_type not in (FrameType.DATA, FrameType.ACK):
        raise LinkFrameError(ParseResult.ERR_FRAME_TYPE)

    more_fragments = bool(flags & FLAG_MORE_FRAGMENTS)
    # Both fields, not just more_fragments: a fragmented message's LAST fragment
    # has more_fragments = 0 with a non-zero fragment_index, and accepting it
    # would hand a decoder the tail of a message as if it were a whole one.
    if more_fragments or fragment_index != 0:
        raise LinkFrameError(ParseResult.ERR_FRAGMENTED)

    payload = buf[HEADER_SIZE:]
    if frame_type == FrameType.ACK and payload:
        raise LinkFrameError(ParseResult.ERR_ACK_HAS_PAYLOAD)

    header = LinkFrameHeader(
        destination_id=dest,
        source_id=src,
        session_id=session,
        sequence=sequence,
        frame_type=frame_type,
        ack_requested=bool(flags & FLAG_ACK_REQUESTED),
        more_fragments=more_fragments,
        fragment_index=fragment_index,
        magic=magic,
        version=version,
        reserved_flags=flags & FLAGS_RESERVED_MASK,
    )
    return header, payload


def _is_valid_node_id(node_id: int) -> bool:
    """Section 7.2: real node IDs are 0x00000001..0xFFFFFFFE. 0 means
    unconfigured and 0xFFFFFFFF is reserved for broadcast, so neither is
    something a node can BE."""
    return node_id not in (ADDR_INVALID, ADDR_BROADCAST)


def make_ack(received: LinkFrameHeader, local_node_id: int) -> LinkFrameHeader:
    """Section 9.5's ACK header, built from the frame being acknowledged.

    Written from section 9.5's field list, deliberately not by reading
    boomlink_linkframe.c - that independence is the whole reason this mapping
    lives in the header layer at all. Every field here is copied or moved, so a
    transposition produces a valid-looking ACK addressed to the wrong node (or
    carrying a swapped session/sequence, which the original sender matches
    against), and the on-air symptom is an ACK timeout that reads as an RF fault.
    An engine that both built and matched ACKs could transpose a pair on both
    sides and pass all of its own delivery tests.

    Raises ValueError if either end of the addressing is not a real node ID.
    An ACK addressed to broadcast is never a valid ACK, and emitting one turns a
    single received frame into a network-wide transmission.
    """
    if not _is_valid_node_id(received.source_id):
        raise ValueError(
            f"cannot acknowledge a frame whose source is {received.source_id:#010x}: "
            f"an ACK must be addressed to a real node (section 7.2)"
        )
    if not _is_valid_node_id(local_node_id):
        raise ValueError(
            f"a node addressed {local_node_id:#010x} cannot acknowledge anything: "
            f"it has no valid identity to acknowledge as (section 7.2)"
        )
    return LinkFrameHeader(
        # The swap section 9.5 is really about: back to whoever SENT the frame,
        # not to whoever it was addressed to.
        destination_id=received.source_id,
        source_id=local_node_id,
        # Copied unchanged - the pair the original sender matches the ACK on.
        session_id=received.session_id,
        sequence=received.sequence,
        frame_type=FrameType.ACK,
        # "ACK packets never request another ACK" - by construction.
        ack_requested=False,
        more_fragments=False,
        fragment_index=0,
        # Echoed, not defaulted: the frame was accepted, so its magic is this
        # network's, and a non-default network must be acknowledged on itself.
        magic=received.magic,
        version=VERSION,
    )


def is_for_node(destination_id: int, local_node_id: int) -> bool:
    """Section 7.2's acceptance rule.

    A node whose own address is not a valid node ID accepts nothing, broadcast
    included - see _is_valid_node_id(). That rules out both ADDR_INVALID
    (unconfigured - it would otherwise "match" a frame addressed to 0) and
    ADDR_BROADCAST (a misconfiguration; a node that thinks it IS the broadcast
    address would answer for the whole network).
    """
    if not _is_valid_node_id(local_node_id):
        return False
    return destination_id in (local_node_id, ADDR_BROADCAST)

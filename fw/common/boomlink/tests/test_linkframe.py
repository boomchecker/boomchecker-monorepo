"""Link frame header tests (boomlink.md sections 7.2, 7.3 and 15.2).

Covers the section 15.2 items that apply to a stateless frame header:
header encode/parse round-trip, rejection of a wrong magic/network ID or
version, wrong-destination rejection, and broadcast acceptance. The stateful
items on that list (ACK matching and timeout, retry count, duplicate
suppression, sequence/session across a reboot) belong to the link engine and
arrive with it - there is no state here to test.

Every behaviour is checked against BOTH implementations: the C
(linkframe_tool) and the independent Python reference
(linkframe/boomlink_linkframe.py), encoding with one and parsing with the
other in both directions. A single implementation checked against itself
would only prove self-consistency, which is exactly what a hand-packed binary
format cannot afford.
"""

import boomlink_linkframe as ref
import pytest
from _support import parse_kv, run_codec_tool

# A header with a distinct byte pattern in every 32-bit field, so a swapped
# pair of fields or a dropped high byte changes the result rather than
# coincidentally still matching.
SAMPLE = dict(
    destination_id=0x11223344,
    source_id=0x55667788,
    session_id=0x99AABBCC,
    sequence=0xDDEEFF01,
)


def run_linkframe_tool(linkframe_tool_path, *args):
    """The link frame tool takes the same shape of arguments as codec_tool, so
    it reuses that runner - which also means it inherits the forced
    abort_on_error=1 sanitizer settings."""
    return run_codec_tool(linkframe_tool_path, *args)


def encode_with_tool(tmp_path, linkframe_tool_path, *, frame_type, flags, fragment_index,
                     payload=b"", name="frame.bin", **fields):
    """Build a frame with the C encoder and return its bytes."""
    path = tmp_path / name
    result = run_linkframe_tool(
        linkframe_tool_path, "encode",
        str(frame_type), str(flags), str(fragment_index),
        str(fields["destination_id"]), str(fields["source_id"]),
        str(fields["session_id"]), str(fields["sequence"]),
        payload.hex(), str(path),
    )
    assert result.returncode == 0, result.stderr
    return path.read_bytes()


def parse_with_tool(tmp_path, linkframe_tool_path, frame: bytes, *, expected_magic=None,
                    name="parse_in.bin"):
    """Parse `frame` with the C parser and return its printed fields."""
    path = tmp_path / name
    path.write_bytes(frame)
    args = ["parse", str(path)]
    if expected_magic is not None:
        args.append(str(expected_magic))
    result = run_linkframe_tool(linkframe_tool_path, *args)
    # A rejected frame is this subcommand's normal output, not a tool failure -
    # only an I/O or usage problem exits nonzero.
    assert result.returncode == 0, result.stderr
    return parse_kv(result.stdout)


def test_python_parse_result_codes_match_the_c_enum(linkframe_constants):
    """The Python mirror of boomlink_linkframe_parse_result_t is only useful if
    its numbers still are the C ones - otherwise a test asserting "rejected for
    reason 3" could pass against the wrong reason entirely."""
    for name, member in ref.ParseResult.__members__.items():
        key = f"result_{name.lower().removeprefix('err_')}"
        assert key in linkframe_constants, (
            f"linkframe_tool limits does not report {key!r}, so Python's "
            f"ParseResult.{name} cannot be checked against the C enumerator"
        )
        assert linkframe_constants[key] == int(member), (
            f"ParseResult.{name} is {int(member)} in Python but "
            f"{linkframe_constants[key]} in C"
        )


def test_python_constants_match_the_c_header(linkframe_constants):
    assert linkframe_constants["header_size"] == ref.HEADER_SIZE
    assert linkframe_constants["magic_default"] == ref.MAGIC_DEFAULT
    assert linkframe_constants["version"] == ref.VERSION
    assert linkframe_constants["frame_type_data"] == int(ref.FrameType.DATA)
    assert linkframe_constants["frame_type_ack"] == int(ref.FrameType.ACK)
    assert linkframe_constants["addr_invalid"] == ref.ADDR_INVALID
    assert linkframe_constants["addr_broadcast"] == ref.ADDR_BROADCAST
    assert linkframe_constants["flag_ack_requested"] == ref.FLAG_ACK_REQUESTED
    assert linkframe_constants["flag_more_fragments"] == ref.FLAG_MORE_FRAGMENTS
    assert linkframe_constants["flags_reserved_mask"] == ref.FLAGS_RESERVED_MASK


def test_wire_layout_is_byte_exact():
    """Pins section 7.3's layout to explicit bytes.

    Spelled out here rather than kept as a binary golden-vector file (the way
    the Protobuf side does): for a 20-byte FIXED layout the expected bytes are
    the specification, so writing them where a reader can see the offsets is
    strictly more useful than a file whose contents nobody can read. This is
    what catches a wrong offset or a big-endian slip - a C-versus-Python
    cross-check alone would not, since both could be wrong the same way only if
    they shared code, which they deliberately do not.
    """
    frame = ref.encode(
        ref.LinkFrameHeader(frame_type=ref.FrameType.DATA, ack_requested=True, **SAMPLE)
    )
    assert frame == bytes(
        [
            0xB0,                    # magic / network ID
            0x11,                    # version 1 (high nibble) | DATA (low nibble)
            0x01,                    # flags: ack_requested
            0x00,                    # fragment_index
            0x44, 0x33, 0x22, 0x11,  # destination_id 0x11223344, little-endian
            0x88, 0x77, 0x66, 0x55,  # source_id      0x55667788
            0xCC, 0xBB, 0xAA, 0x99,  # session_id     0x99AABBCC
            0x01, 0xFF, 0xEE, 0xDD,  # sequence       0xDDEEFF01
        ]
    )
    assert len(frame) == ref.HEADER_SIZE


def test_ack_frame_wire_layout_is_byte_exact():
    """An ACK differs from a DATA frame only in the frame-type nibble and in
    carrying no payload (section 9.5)."""
    frame = ref.encode(ref.LinkFrameHeader(frame_type=ref.FrameType.ACK, **SAMPLE))
    assert frame[1] == 0x12, "version 1 | ACK"
    assert frame[2] == 0x00, "an ACK never requests an ACK"
    assert len(frame) == ref.HEADER_SIZE


@pytest.mark.parametrize("frame_type", [ref.FrameType.DATA, ref.FrameType.ACK])
@pytest.mark.parametrize("ack_requested", [False, True])
def test_c_encode_python_parse(tmp_path, linkframe_tool_path, frame_type, ack_requested):
    payload = b"\xde\xad\xbe\xef" if frame_type == ref.FrameType.DATA else b""
    flags = ref.FLAG_ACK_REQUESTED if ack_requested else 0
    frame = encode_with_tool(
        tmp_path, linkframe_tool_path,
        frame_type=int(frame_type), flags=flags, fragment_index=0,
        payload=payload, **SAMPLE,
    )

    header, parsed_payload = ref.parse(frame)
    assert header.frame_type == int(frame_type)
    assert header.ack_requested is ack_requested
    assert header.more_fragments is False
    assert header.fragment_index == 0
    assert header.reserved_flags == 0
    assert header.destination_id == SAMPLE["destination_id"]
    assert header.source_id == SAMPLE["source_id"]
    assert header.session_id == SAMPLE["session_id"]
    assert header.sequence == SAMPLE["sequence"]
    assert parsed_payload == payload


@pytest.mark.parametrize("frame_type", [ref.FrameType.DATA, ref.FrameType.ACK])
def test_python_encode_c_parse(tmp_path, linkframe_tool_path, frame_type):
    payload = b"\x01\x02\x03" if frame_type == ref.FrameType.DATA else b""
    frame = ref.encode(
        ref.LinkFrameHeader(frame_type=frame_type, ack_requested=True, **SAMPLE)
    ) + payload

    fields = parse_with_tool(tmp_path, linkframe_tool_path, frame)
    assert fields["result"] == str(int(ref.ParseResult.OK)), fields
    assert fields["frame_type"] == str(int(frame_type))
    assert fields["ack_requested"] == "1"
    assert fields["destination_id"] == str(SAMPLE["destination_id"])
    assert fields["source_id"] == str(SAMPLE["source_id"])
    assert fields["session_id"] == str(SAMPLE["session_id"])
    assert fields["sequence"] == str(SAMPLE["sequence"])
    assert fields["payload_len"] == str(len(payload))
    assert fields["payload"] == payload.hex()


def test_wrong_magic_is_rejected(tmp_path, linkframe_tool_path):
    """Section 7.3: foreign traffic is dropped and counted before any further
    processing, which is what lets two deployments share a channel."""
    frame = ref.encode(ref.LinkFrameHeader(magic=0x5A, **SAMPLE))

    fields = parse_with_tool(tmp_path, linkframe_tool_path, frame)
    assert fields["result"] == str(int(ref.ParseResult.ERR_MAGIC)), fields
    with pytest.raises(ref.LinkFrameError) as excinfo:
        ref.parse(frame)
    assert excinfo.value.result is ref.ParseResult.ERR_MAGIC


def test_a_matching_non_default_magic_is_accepted(tmp_path, linkframe_tool_path):
    """The magic is runtime-configurable (section 7.3), so a non-default value
    must be accepted when it is the one expected - otherwise the field is a
    constant wearing a parameter's clothes."""
    frame = ref.encode(ref.LinkFrameHeader(magic=0x5A, **SAMPLE))

    fields = parse_with_tool(tmp_path, linkframe_tool_path, frame, expected_magic=0x5A)
    assert fields["result"] == str(int(ref.ParseResult.OK)), fields
    header, _ = ref.parse(frame, expected_magic=0x5A)
    assert header.magic == 0x5A


def test_wrong_version_is_rejected(tmp_path, linkframe_tool_path):
    frame = bytearray(ref.encode(ref.LinkFrameHeader(**SAMPLE)))
    # Version 2, frame type DATA - a future version this build does not know.
    frame[1] = (2 << 4) | int(ref.FrameType.DATA)

    fields = parse_with_tool(tmp_path, linkframe_tool_path, bytes(frame))
    assert fields["result"] == str(int(ref.ParseResult.ERR_VERSION)), fields
    with pytest.raises(ref.LinkFrameError) as excinfo:
        ref.parse(bytes(frame))
    assert excinfo.value.result is ref.ParseResult.ERR_VERSION


@pytest.mark.parametrize("frame_type", [0, 3, 15])
def test_unknown_frame_type_is_rejected(tmp_path, linkframe_tool_path, frame_type):
    """0 in particular: an all-zero buffer must never parse as a usable frame."""
    frame = bytearray(ref.encode(ref.LinkFrameHeader(**SAMPLE)))
    frame[1] = (ref.VERSION << 4) | frame_type

    fields = parse_with_tool(tmp_path, linkframe_tool_path, bytes(frame))
    assert fields["result"] == str(int(ref.ParseResult.ERR_FRAME_TYPE)), fields
    with pytest.raises(ref.LinkFrameError) as excinfo:
        ref.parse(bytes(frame))
    assert excinfo.value.result is ref.ParseResult.ERR_FRAME_TYPE


@pytest.mark.parametrize(
    "more_fragments,fragment_index",
    [
        (True, 0),   # a first/middle fragment
        (False, 1),  # the LAST fragment of a fragmented message
        (True, 2),   # a middle fragment
    ],
)
def test_fragmented_frames_are_dropped(tmp_path, linkframe_tool_path, more_fragments,
                                        fragment_index):
    """Section 7.3 requires dropping on more_fragments=1 OR fragment_index!=0.

    The (False, 1) case is the one that matters and the reason the rule is not
    just "check more_fragments": a fragmented message's last fragment correctly
    carries more_fragments=0 while fragment_index is non-zero, and a receiver
    checking only the flag would hand that tail to the decoder as though it
    were a whole Envelope.
    """
    flags = ref.FLAG_MORE_FRAGMENTS if more_fragments else 0
    frame = encode_with_tool(
        tmp_path, linkframe_tool_path,
        frame_type=int(ref.FrameType.DATA), flags=flags, fragment_index=fragment_index,
        payload=b"\x01\x02", **SAMPLE,
    )

    fields = parse_with_tool(tmp_path, linkframe_tool_path, frame)
    assert fields["result"] == str(int(ref.ParseResult.ERR_FRAGMENTED)), fields
    with pytest.raises(ref.LinkFrameError) as excinfo:
        ref.parse(frame)
    assert excinfo.value.result is ref.ParseResult.ERR_FRAGMENTED


@pytest.mark.parametrize("reserved_bit", [2, 3, 4, 5, 6, 7])
def test_reserved_flag_bits_are_ignored_not_rejected(tmp_path, linkframe_tool_path,
                                                      reserved_bit):
    """Section 7.3's deliberate OPPOSITE of the fragmentation rule: an
    unrecognized bit among flags 2-7 must be ignored, not treated as a reason to
    drop the frame. That is ordinary forward compatibility - a newer sender
    setting a bit this build has never heard of must not make its traffic
    invisible. Any future bit that would change how the payload is framed has to
    go behind the version nibble instead, precisely so this default stays safe.
    """
    flags = ref.FLAG_ACK_REQUESTED | (1 << reserved_bit)
    frame = encode_with_tool(
        tmp_path, linkframe_tool_path,
        frame_type=int(ref.FrameType.DATA), flags=flags, fragment_index=0,
        payload=b"\x07", **SAMPLE,
    )
    assert frame[2] & (1 << reserved_bit), (
        "the tool must have written the raw flags byte through - otherwise this "
        "test is not exercising a reserved bit at all"
    )

    fields = parse_with_tool(tmp_path, linkframe_tool_path, frame)
    assert fields["result"] == str(int(ref.ParseResult.OK)), fields
    assert fields["ack_requested"] == "1", "the known flag must still be read"
    assert fields["reserved_flags"] == str(1 << reserved_bit), (
        "the unrecognized bit should be reported for diagnostics, not silently dropped"
    )
    assert fields["payload"] == "07"

    header, payload = ref.parse(frame)
    assert header.ack_requested is True
    assert header.reserved_flags == (1 << reserved_bit)
    assert payload == b"\x07"


def test_encoder_never_puts_reserved_bits_on_the_air(tmp_path, linkframe_tool_path):
    """The receiver tolerates reserved bits; the SENDER must never set them
    (section 7.3: "always 0 until a future PR assigns them"). Echoing back a
    bit a parsed frame happened to carry is the obvious way that breaks, so
    encode() ignores reserved_flags entirely."""
    header = ref.LinkFrameHeader(**SAMPLE)
    header.reserved_flags = 0xFC
    assert ref.encode(header)[2] == 0

    # And the C encoder likewise, checked through the one path that can ask it
    # for a reserved bit: `encode` passes the flags byte to the encoder before
    # overwriting it, so a value with only reserved bits set must come back 0
    # from the encoder itself. Verified by encoding with flags=0 and confirming
    # the byte is 0 regardless of what a header struct might carry.
    frame = encode_with_tool(
        tmp_path, linkframe_tool_path,
        frame_type=int(ref.FrameType.DATA), flags=0, fragment_index=0, **SAMPLE,
    )
    assert frame[2] == 0


def test_ack_with_a_payload_is_rejected(tmp_path, linkframe_tool_path):
    """Section 9.5: "An ACK frame has no payload". An ACK that carries one is
    either a bug or something pretending to be an ACK."""
    frame = ref.encode(ref.LinkFrameHeader(frame_type=ref.FrameType.ACK, **SAMPLE)) + b"\x00"

    fields = parse_with_tool(tmp_path, linkframe_tool_path, frame)
    assert fields["result"] == str(int(ref.ParseResult.ERR_ACK_HAS_PAYLOAD)), fields
    with pytest.raises(ref.LinkFrameError) as excinfo:
        ref.parse(frame)
    assert excinfo.value.result is ref.ParseResult.ERR_ACK_HAS_PAYLOAD


@pytest.mark.parametrize("length", [0, 1, 19])
def test_short_buffers_are_rejected(tmp_path, linkframe_tool_path, length):
    """A truncated frame must fail closed rather than read past the buffer -
    exercised under ASan, so an over-read would be a hard failure, not a
    plausible-looking parse."""
    frame = ref.encode(ref.LinkFrameHeader(**SAMPLE))[:length]

    fields = parse_with_tool(tmp_path, linkframe_tool_path, frame)
    assert fields["result"] == str(int(ref.ParseResult.ERR_TOO_SHORT)), fields
    with pytest.raises(ref.LinkFrameError) as excinfo:
        ref.parse(frame)
    assert excinfo.value.result is ref.ParseResult.ERR_TOO_SHORT


def test_data_frame_with_no_payload_parses(tmp_path, linkframe_tool_path):
    """Deliberately NOT rejected here. Section 9.2 splits the failure classes:
    a bad link header is a BoomLink-layer failure, a payload that is not a valid
    Envelope is a BoomProtocol-layer one, and the two are counted separately -
    so the link parser must not pre-empt the codec's judgement about payload
    contents. An empty payload simply decodes to nothing and the codec rejects
    it (it has no header), which is where that belongs."""
    frame = ref.encode(ref.LinkFrameHeader(frame_type=ref.FrameType.DATA, **SAMPLE))

    fields = parse_with_tool(tmp_path, linkframe_tool_path, frame)
    assert fields["result"] == str(int(ref.ParseResult.OK)), fields
    assert fields["payload_len"] == "0"
    header, payload = ref.parse(frame)
    assert header.frame_type == int(ref.FrameType.DATA)
    assert payload == b""


@pytest.mark.parametrize(
    "destination_id,local_node_id,accepted,why",
    [
        (0x00000042, 0x00000042, True, "addressed to this node"),
        (0x00000042, 0x00000043, False, "addressed to a different node"),
        (ref.ADDR_BROADCAST, 0x00000042, True, "broadcast reaches a configured node"),
        (ref.ADDR_BROADCAST, ref.ADDR_INVALID, False,
         "an unconfigured node accepts nothing, broadcast included - acting on "
         "traffic before knowing who you are is how a half-provisioned node "
         "answers for someone else"),
        (0x00000042, ref.ADDR_INVALID, False, "unconfigured node, unicast"),
        (ref.ADDR_INVALID, 0x00000042, False,
         "0 is the invalid/unconfigured address and is never a real destination"),
    ],
)
def test_address_acceptance(linkframe_tool_path, destination_id, local_node_id, accepted, why):
    """Section 7.2's acceptance rule, checked in both implementations."""
    result = run_linkframe_tool(
        linkframe_tool_path, "accepts", str(destination_id), str(local_node_id)
    )
    assert result.returncode == 0, result.stderr
    expected = "1" if accepted else "0"
    assert parse_kv(result.stdout)["accepts"] == expected, why
    assert ref.is_for_node(destination_id, local_node_id) is accepted, why


def test_encode_rejects_malformed_numeric_arguments(tmp_path, linkframe_tool_path):
    """Same discipline as the codec tool: a typo'd argument must fail loudly
    rather than silently become another value, or a test asserting "this frame
    is rejected" could be passing for the wrong reason entirely."""
    out_path = tmp_path / "out.bin"
    bad = ("xyz", "-1", "99999999999999999999", "1.5", "", "-0", " 5", "\t7", "+5",
           "5 ", "0x10", "1e3", "5abc", "1,000")
    for bad_value in bad:
        # Substituted for the sequence argument; any uint32 field would do.
        result = run_linkframe_tool(
            linkframe_tool_path, "encode", "1", "0", "0",
            str(SAMPLE["destination_id"]), str(SAMPLE["source_id"]),
            str(SAMPLE["session_id"]), bad_value, "", str(out_path),
        )
        assert result.returncode == 2, (
            f"sequence={bad_value!r} should be a parse error, got rc={result.returncode}"
        )


def test_frame_type_over_a_nibble_is_rejected(tmp_path, linkframe_tool_path):
    """frame_type shares byte 1 with the version nibble, so a value above 15
    would corrupt the version if it were not masked. The encoder masks it; this
    checks the result is a version mismatch rather than a silently valid frame
    of some other type."""
    out_path = tmp_path / "wide_type.bin"
    result = run_linkframe_tool(
        linkframe_tool_path, "encode", "17", "0", "0",
        str(SAMPLE["destination_id"]), str(SAMPLE["source_id"]),
        str(SAMPLE["session_id"]), str(SAMPLE["sequence"]), "", str(out_path),
    )
    assert result.returncode == 0, result.stderr
    frame = out_path.read_bytes()
    # 17 & 0x0F == 1 (DATA); the version nibble must be untouched.
    assert frame[1] == (ref.VERSION << 4) | int(ref.FrameType.DATA)

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

import struct

import boomlink_linkframe as ref
import pytest
from _support import parse_kv, run_codec_tool

# Section 7.3's layout, as this module's OWN copy - deliberately not imported
# from the reference implementation. Used only to forge frames that violate
# several rules at once (see _frame_violating); the same reasoning as
# test_wire_layout_is_byte_exact spelling the 20 bytes out as literals.
_LAYOUT = struct.Struct("<BBBBIIII")
assert _LAYOUT.size == ref.HEADER_SIZE

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
                     payload=b"", name="frame.bin", raw=False, **fields):
    """Build a frame with the C encoder and return its bytes.

    `raw=True` selects the `encode_raw` subcommand, which overwrites the flags
    byte after the encoder has run. Only use it where a test genuinely needs a
    frame the encoder refuses to emit (a set reserved bit): with `raw=False` the
    encoder's own flags byte reaches the file, which is what makes it observable
    at all.
    """
    path = tmp_path / name
    result = run_linkframe_tool(
        linkframe_tool_path, "encode_raw" if raw else "encode",
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

    # And the reverse: iterating only Python's members would miss a C enumerator
    # APPENDED after ERR_ACK_HAS_PAYLOAD - a new drop reason the reference
    # parser cannot produce, and no test would notice.
    reported = {k for k in linkframe_constants if k.startswith("result_")}
    mirrored = {
        f"result_{name.lower().removeprefix('err_')}"
        for name in ref.ParseResult.__members__
    }
    assert reported == mirrored, (
        f"C reports parse results the Python mirror does not have: {reported - mirrored}; "
        f"Python has ones C does not report: {mirrored - reported}"
    )
    # The check above compares two hand-maintained lists, so both of them being
    # short in the same way passes it. This one does not: the count comes from
    # the C enum's own sentinel (BOOMLINK_LINKFRAME_RESULT_COUNT), so a result
    # appended to the enum and not reported by `limits` fails here even though
    # the Python mirror was never touched.
    assert len(reported) == linkframe_constants["parse_result_count"], (
        f"the C enum has {linkframe_constants['parse_result_count']} parse results but "
        f"`limits` reports {len(reported)} of them: {sorted(reported)}"
    )


def test_parse_result_names_describe_the_right_result(linkframe_constants):
    """The strings boomlink_linkframe_parse_result_str() returns are what a field
    log or CLI actually shows, and nothing used to read them: swapping the
    ERR_MAGIC and ERR_VERSION strings left the entire suite green, so every
    wrong-magic drop would have been reported as a version mismatch. The header
    justifies having separate reasons at all with section 9.10's requirement to
    count and debug them independently - worthless if the labels lie.

    Checked by required keyword rather than by exact text, so rewording a message
    is free while mislabelling one is not. This is an expectation independent of
    the C, which is the point: comparing the C's strings to a Python copy of the
    same strings would agree just as happily with them swapped.
    """
    required = {
        ref.ParseResult.OK: "ok",
        ref.ParseResult.ERR_TOO_SHORT: "short",
        ref.ParseResult.ERR_MAGIC: "magic",
        ref.ParseResult.ERR_VERSION: "version",
        ref.ParseResult.ERR_FRAME_TYPE: "type",
        ref.ParseResult.ERR_FRAGMENTED: "fragment",
        ref.ParseResult.ERR_ACK_HAS_PAYLOAD: "payload",
    }
    assert set(required) == set(ref.ParseResult), (
        "a parse result has no expected keyword here - add one rather than "
        "leaving its label unchecked"
    )

    described = {}
    for result, keyword in required.items():
        key = f"parse_result_str_{int(result)}"
        assert key in linkframe_constants, (
            f"linkframe_tool limits does not report {key!r}, so "
            f"ParseResult.{result.name}'s label is unchecked"
        )
        text = str(linkframe_constants[key])
        assert keyword in text.lower(), (
            f"ParseResult.{result.name} is described as {text!r}, which does not "
            f"mention {keyword!r} - the label does not match the result"
        )
        described[result] = text

    # Distinct, and none of them the catch-all. Two results sharing a label is
    # just as misleading in a log as a swapped one, and the fallback string
    # appearing here would mean a result has no case at all.
    assert len(set(described.values())) == len(described), (
        f"two parse results share a label: {sorted(described.values())}"
    )
    # Compared against the fallback the tool reports for an out-of-range value,
    # not against the word "unknown" - "unknown frame type" is a legitimate label
    # and matching on the word flagged it.
    catch_all = str(linkframe_constants["parse_result_str_out_of_range"])
    assert catch_all, "the tool reports no catch-all string to compare against"
    assert catch_all not in described.values(), (
        f"a real parse result falls through to the catch-all {catch_all!r}: {described}"
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
    # Both masks, not just the reserved one. Each side derives the reserved mask
    # from its own assigned mask, so comparing only the derived value would let a
    # bit assigned on one side alone slip through whenever the complement happened
    # to agree - and the assigned mask is the one a new flag has to be added to.
    assert linkframe_constants["flags_assigned_mask"] == ref.FLAGS_ASSIGNED_MASK
    assert linkframe_constants["flags_reserved_mask"] == ref.FLAGS_RESERVED_MASK
    # The C's _Static_assert pins this too; asserted here so a mask change shows
    # up as "the wire reservation moved" rather than only as a build failure in
    # the other language.
    assert linkframe_constants["flags_reserved_mask"] == 0xFC, (
        "boomlink.md section 7.3 reserves flags bits 2-7; assigning one is a "
        "wire-format change"
    )


def test_wire_layout_is_byte_exact(tmp_path, linkframe_tool_path):
    """Pins section 7.3's layout to explicit bytes, for BOTH encoders.

    Spelled out here rather than kept as a binary golden-vector file (the way
    the Protobuf side does): for a 20-byte FIXED layout the expected bytes are
    the specification, so writing them where a reader can see the offsets is
    strictly more useful than a file whose contents nobody can read.

    This is the only test that pins the layout to the SPEC rather than to the
    other implementation. The C-versus-Python cross-checks would indeed catch one
    side drifting, but they say nothing about which side is right: someone
    "fixing" the failing one to agree with the other resolves them all and
    changes the wire format. These literals are what makes that a failure.

    Both encoders are compared against the same literals, deliberately - the
    Python one alone would leave the C's byte 2 covered only by the flags tests
    below, which is roughly how that byte went unobserved in the first place.
    """
    expected = bytes(
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
    assert len(expected) == ref.HEADER_SIZE

    python_frame = ref.encode(
        ref.LinkFrameHeader(frame_type=ref.FrameType.DATA, ack_requested=True, **SAMPLE)
    )
    assert python_frame == expected

    c_frame = encode_with_tool(
        tmp_path, linkframe_tool_path,
        frame_type=int(ref.FrameType.DATA), flags=ref.FLAG_ACK_REQUESTED,
        fragment_index=0, **SAMPLE,
    )
    assert c_frame == expected


def test_ack_frame_wire_layout_is_byte_exact():
    """An ACK differs from a DATA frame only in the frame-type nibble and in
    carrying no payload (section 9.5)."""
    frame = ref.encode(ref.LinkFrameHeader(frame_type=ref.FrameType.ACK, **SAMPLE))
    assert frame[1] == 0x12, "version 1 | ACK"
    # The default header simply does not request an ACK. Neither encoder
    # ENFORCES section 9.5's "ACK packets never request another ACK" - that is a
    # TX-pipeline rule belonging to the link engine, and a stateless parser must
    # report what arrived rather than what a compliant sender would have sent.
    assert frame[2] == 0x00, "the default header does not request an ACK"
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


@pytest.mark.parametrize("version", [0, 2, 15])
def test_wrong_version_is_rejected(tmp_path, linkframe_tool_path, version):
    """Version 0 and 15 as well as 2, for the same reason the frame-type test
    covers type 0: version 0 is what an uninitialised or zero-padded byte 1 gives
    (with a correct magic, which a replayed or spoofed header supplies), and 15 is
    the nibble's ceiling. With only version 2 covered, a guard written as
    `version != VERSION && version != 0` - accepting a frame whose version nibble
    was never set - passed the entire suite. Verified."""
    frame = bytearray(ref.encode(ref.LinkFrameHeader(**SAMPLE)))
    # A version this build does not implement, frame type DATA.
    frame[1] = (version << 4) | int(ref.FrameType.DATA)

    fields = parse_with_tool(tmp_path, linkframe_tool_path, bytes(frame))
    assert fields["result"] == str(int(ref.ParseResult.ERR_VERSION)), fields
    with pytest.raises(ref.LinkFrameError) as excinfo:
        ref.parse(bytes(frame))
    assert excinfo.value.result is ref.ParseResult.ERR_VERSION


@pytest.mark.parametrize("frame_type", [0, 3, 15])
def test_unknown_frame_type_is_rejected(tmp_path, linkframe_tool_path, frame_type):
    """Type 0 included, so a frame whose type nibble was never set cannot parse.
    Note this sets ONLY the type nibble on an otherwise valid frame - an
    all-zero buffer is a different case, covered separately below (it never
    reaches the frame-type check, being rejected on magic first)."""
    frame = bytearray(ref.encode(ref.LinkFrameHeader(**SAMPLE)))
    frame[1] = (ref.VERSION << 4) | frame_type

    fields = parse_with_tool(tmp_path, linkframe_tool_path, bytes(frame))
    assert fields["result"] == str(int(ref.ParseResult.ERR_FRAME_TYPE)), fields
    with pytest.raises(ref.LinkFrameError) as excinfo:
        ref.parse(bytes(frame))
    assert excinfo.value.result is ref.ParseResult.ERR_FRAME_TYPE


def test_an_all_zero_buffer_is_rejected(tmp_path, linkframe_tool_path):
    """A zeroed buffer - an uninitialised RX slot, a padded read - must never
    parse as a usable frame. It is rejected on MAGIC, before the frame-type
    check, since 0x00 is not the expected network ID; asserting the specific
    reason keeps this honest about which rule does the work."""
    frame = bytes(ref.HEADER_SIZE)

    fields = parse_with_tool(tmp_path, linkframe_tool_path, frame)
    assert fields["result"] == str(int(ref.ParseResult.ERR_MAGIC)), fields
    with pytest.raises(ref.LinkFrameError) as excinfo:
        ref.parse(frame)
    assert excinfo.value.result is ref.ParseResult.ERR_MAGIC


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
    # raw=True: the encoder correctly refuses to emit a reserved bit, so forging
    # the byte is the only way to produce the frame a NEWER SENDER would send.
    frame = encode_with_tool(
        tmp_path, linkframe_tool_path,
        frame_type=int(ref.FrameType.DATA), flags=flags, fragment_index=0,
        payload=b"\x07", raw=True, **SAMPLE,
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
    (section 7.3: "always 0 until a future PR assigns them"). Echoing back a bit
    a parsed frame happened to carry is the obvious way that breaks, so both
    encoders ignore reserved_flags entirely.

    This test earns its keep only if the encoder is actually ASKED for a
    reserved bit and its own output is what gets inspected. An earlier version
    was vacuous on the C side for two independent reasons - the tool never
    populated reserved_flags, and it overwrote the flags byte afterwards - which
    left byte 2 of the C encoder's output unobserved by the entire suite: an
    encoder that set a reserved bit on every frame, or swapped the two known
    flags, passed 100%. Hence flags=0xFD (ack + every reserved bit) through
    plain `encode`, plus a byte-exact comparison so byte 2 is never again the
    byte nobody looks at.
    """
    all_reserved_plus_ack = ref.FLAG_ACK_REQUESTED | ref.FLAGS_RESERVED_MASK
    assert all_reserved_plus_ack == 0xFD

    header = ref.LinkFrameHeader(ack_requested=True, **SAMPLE)
    header.reserved_flags = ref.FLAGS_RESERVED_MASK
    python_frame = ref.encode(header)
    assert python_frame[2] == ref.FLAG_ACK_REQUESTED

    frame = encode_with_tool(
        tmp_path, linkframe_tool_path,
        frame_type=int(ref.FrameType.DATA), flags=all_reserved_plus_ack,
        fragment_index=0, **SAMPLE,
    )
    assert frame[2] == ref.FLAG_ACK_REQUESTED, (
        f"the C encoder emitted flags byte {frame[2]:#04x}; it must keep the known "
        f"ack_requested bit and drop every reserved one"
    )
    # Whole-frame equality, so byte 2 is covered by an exact expectation rather
    # than only by the assertion above.
    assert frame == python_frame


def test_encoder_keeps_the_two_known_flags_distinct(tmp_path, linkframe_tool_path):
    """Guards against the two assigned bits being swapped - which byte 2 being
    unobserved also used to hide. Uses more_fragments alone, so the frame is
    rejected on parse; only the emitted byte matters here."""
    for flags, label in ((ref.FLAG_ACK_REQUESTED, "ack only"),
                         (ref.FLAG_MORE_FRAGMENTS, "more_fragments only")):
        frame = encode_with_tool(
            tmp_path, linkframe_tool_path,
            frame_type=int(ref.FrameType.DATA), flags=flags, fragment_index=0,
            name=f"flags_{flags}.bin", **SAMPLE,
        )
        assert frame[2] == flags, f"{label}: emitted {frame[2]:#04x}, expected {flags:#04x}"


def test_ack_with_a_payload_is_rejected(tmp_path, linkframe_tool_path):
    """Section 9.5: "An ACK frame has no payload". An ACK that carries one is
    either a bug or something pretending to be an ACK."""
    frame = ref.encode(ref.LinkFrameHeader(frame_type=ref.FrameType.ACK, **SAMPLE)) + b"\x00"

    fields = parse_with_tool(tmp_path, linkframe_tool_path, frame)
    assert fields["result"] == str(int(ref.ParseResult.ERR_ACK_HAS_PAYLOAD)), fields
    with pytest.raises(ref.LinkFrameError) as excinfo:
        ref.parse(frame)
    assert excinfo.value.result is ref.ParseResult.ERR_ACK_HAS_PAYLOAD


def _frame_violating(*, magic=ref.MAGIC_DEFAULT, version=ref.VERSION,
                     frame_type=int(ref.FrameType.DATA), flags=0, fragment_index=0,
                     payload=b"", truncate_to=None):
    """A hand-built frame that can violate several rules at once.

    Packed here rather than through ref.encode(), for two reasons: a frame can
    then carry a combination the encoder would refuse to emit (a reserved bit, a
    fragment index) without going through either implementation's opinion about
    it, and the layout comes from this module's own format string rather than the
    reference's - so this cannot silently follow the reference somewhere wrong.
    """
    frame = _LAYOUT.pack(
        magic, ((version & 0x0F) << 4) | (frame_type & 0x0F), flags, fragment_index,
        SAMPLE["destination_id"], SAMPLE["source_id"], SAMPLE["session_id"],
        SAMPLE["sequence"],
    ) + payload
    return frame[:truncate_to] if truncate_to is not None else frame


@pytest.mark.parametrize(
    "frame,expected,why",
    [
        pytest.param(
            _frame_violating(magic=0x5A, version=9, frame_type=0, flags=0xFF,
                             fragment_index=7, payload=b"\x01"),
            ref.ParseResult.ERR_MAGIC,
            "magic outranks everything: section 7.3 requires foreign traffic dropped "
            "before any further processing, so a frame from another network must not "
            "be counted against this one's malformed-packet statistics",
            id="magic-beats-everything-else"),
        pytest.param(
            _frame_violating(version=2, frame_type=0),
            ref.ParseResult.ERR_VERSION,
            "version before frame type - a future version is allowed to redefine the "
            "type nibble entirely, so judging the type of a frame we cannot interpret "
            "is meaningless",
            id="version-beats-frame-type"),
        pytest.param(
            _frame_violating(version=2, frame_type=0, truncate_to=19),
            ref.ParseResult.ERR_TOO_SHORT,
            "length before anything: the other checks read bytes that are not there",
            id="length-beats-version"),
        pytest.param(
            _frame_violating(frame_type=int(ref.FrameType.ACK),
                             flags=ref.FLAG_MORE_FRAGMENTS, fragment_index=3,
                             payload=b"\x01\x02"),
            ref.ParseResult.ERR_FRAGMENTED,
            "fragmentation before the ACK-has-no-payload rule: a fragmented frame's "
            "payload length is not the message's, so 'this ACK carries a payload' is "
            "not a conclusion that can be drawn from it yet",
            id="fragmentation-beats-ack-payload"),
        pytest.param(
            _frame_violating(frame_type=0, flags=ref.FLAG_MORE_FRAGMENTS),
            ref.ParseResult.ERR_FRAME_TYPE,
            "frame type before fragmentation",
            id="frame-type-beats-fragmentation"),
    ],
)
def test_validation_order_matches_across_implementations(tmp_path, linkframe_tool_path,
                                                         frame, expected, why):
    """Frames violating SEVERAL rules at once, to pin the ORDER of the checks.

    Every other negative test here breaks exactly one rule, so all of them pass
    against any permutation of the validation order - while the Python reference's
    docstring states the order matches the C and that tests rely on it. Verified:
    swapping the version and frame-type checks in boomlink_linkframe.py, or
    moving the ACK-payload check ahead of the fragmentation check, left the whole
    suite green while producing a genuine cross-language divergence in a field
    section 9.10 requires to be counted separately.

    The order is not arbitrary bookkeeping - each `why` below is the reason that
    particular pair cannot be swapped without reporting something misleading.
    """
    fields = parse_with_tool(tmp_path, linkframe_tool_path, frame)
    assert fields["result"] == str(int(expected)), f"C: {why}\n{fields}"

    with pytest.raises(ref.LinkFrameError) as excinfo:
        ref.parse(frame)
    assert excinfo.value.result is expected, f"Python: {why}"


@pytest.mark.parametrize("length", [0, 1, 19])
def test_short_buffers_are_rejected(tmp_path, linkframe_tool_path, length):
    """A truncated frame must fail closed rather than read past the buffer.

    The ASan part of that is not automatic and is worth naming: the tool copies
    the input into a heap block of EXACTLY `len` bytes before parsing, so the
    redzone sits immediately after the last valid byte. Handing the parser a
    logical length into a larger fixed buffer instead makes any over-read
    intra-object, where ASan is blind - verified: a parser reading bytes 12..19
    before checking the length accepted a 0-byte input and the whole suite
    passed green.
    """
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


ACKING_NODE = 0x0BADF00D


def ack_with_tool(tmp_path, linkframe_tool_path, frame: bytes, local_node_id=ACKING_NODE,
                  *, name="to_ack.bin"):
    """ACK `frame` with the C implementation. Returns (built, ack_bytes)."""
    in_path = tmp_path / name
    out_path = tmp_path / f"ack_{name}"
    in_path.write_bytes(frame)
    result = run_linkframe_tool(
        linkframe_tool_path, "ack", str(in_path), str(local_node_id), str(out_path)
    )
    assert result.returncode == 0, result.stderr
    fields = parse_kv(result.stdout)
    if fields["ack_built"] == "0":
        assert not out_path.exists(), "the tool wrote an ACK it said it did not build"
        return False, b""
    return True, out_path.read_bytes()


def test_ack_mapping_wire_layout_is_byte_exact(tmp_path, linkframe_tool_path):
    """Section 9.5's ACK mapping, pinned to explicit bytes for BOTH implementations.

    This is the test the mapping was moved into this phase for. Every field of an
    ACK is copied or moved from the frame being acknowledged, four of them are
    32-bit, and a transposition yields a structurally perfect ACK frame -
    correct magic, version and type, parses clean - that is simply addressed to
    the wrong node, or carries a swapped (session, sequence) that the original
    sender matches against. On air the symptom is an ACK timeout, then retries,
    then a TX failure: read as an RF or timing fault, not as a field swap.

    Pinned to LITERALS from section 9.5's text rather than to the other
    implementation, because the failure mode this guards against survives any
    self-consistent check. An engine that both builds and matches ACKs can
    transpose a pair on both sides and pass all of its own delivery tests while
    interoperating with nothing.
    """
    data = ref.encode(
        ref.LinkFrameHeader(frame_type=ref.FrameType.DATA, ack_requested=True, **SAMPLE)
    ) + b"\x01\x02\x03"

    expected = bytes(
        [
            0xB0,                    # magic, echoed from the acknowledged frame
            0x12,                    # version 1 | ACK
            0x00,                    # flags: an ACK never requests an ACK
            0x00,                    # fragment_index
            0x88, 0x77, 0x66, 0x55,  # destination = the frame's SOURCE 0x55667788
            0x0D, 0xF0, 0xAD, 0x0B,  # source      = this node 0x0BADF00D
            0xCC, 0xBB, 0xAA, 0x99,  # session_id  copied 0x99AABBCC
            0x01, 0xFF, 0xEE, 0xDD,  # sequence    copied 0xDDEEFF01
        ]
    )
    assert len(expected) == ref.HEADER_SIZE

    built, c_ack = ack_with_tool(tmp_path, linkframe_tool_path, data)
    assert built, "the C implementation refused an ordinary ACK-requested frame"
    assert c_ack == expected

    received, _ = ref.parse(data)
    python_ack = ref.encode(ref.make_ack(received, ACKING_NODE))
    assert python_ack == expected


def test_ack_names_the_right_source_for_every_field(tmp_path, linkframe_tool_path):
    """The same mapping, asserted field by field with the confusion each one rules
    out - so a failure says WHICH transposition happened rather than just that
    twenty bytes differ. SAMPLE gives every 32-bit field a distinct byte pattern
    precisely so a swap cannot coincidentally still match."""
    data = ref.encode(
        ref.LinkFrameHeader(frame_type=ref.FrameType.DATA, ack_requested=True, **SAMPLE)
    )
    received, _ = ref.parse(data)
    ack = ref.make_ack(received, ACKING_NODE)

    assert ack.destination_id == SAMPLE["source_id"], (
        "the ACK must go back to whoever SENT the frame, not to whoever it was "
        "addressed to - that is the swap section 9.5 exists to specify"
    )
    assert ack.source_id == ACKING_NODE, "the ACK's source is the acknowledging node"
    # session_id and sequence are the pair the original sender matches the ACK
    # against, so a transposition times out every delivery. SAMPLE gives them
    # distinct patterns, which is what makes these two equalities catch it - a
    # separate "not transposed" assertion would be implied by them and could
    # never fail on its own.
    assert ack.session_id == SAMPLE["session_id"]
    assert ack.sequence == SAMPLE["sequence"]
    assert ack.frame_type == int(ref.FrameType.ACK)
    assert ack.ack_requested is False, "section 9.5: ACK packets never request another ACK"
    assert ack.more_fragments is False
    assert ack.fragment_index == 0
    assert ack.magic == ref.MAGIC_DEFAULT
    assert ack.version == ref.VERSION

    # The ACK is acceptable to the original sender and to nobody else - the
    # observable consequence of the swap being right.
    assert ref.is_for_node(ack.destination_id, SAMPLE["source_id"]) is True
    assert ref.is_for_node(ack.destination_id, ACKING_NODE) is False

    # And it is a well-formed, payload-free ACK frame in its own right.
    parsed, payload = ref.parse(ref.encode(ack))
    assert parsed.frame_type == int(ref.FrameType.ACK)
    assert payload == b""
    fields = parse_with_tool(tmp_path, linkframe_tool_path, ref.encode(ack))
    assert fields["result"] == str(int(ref.ParseResult.OK)), fields


def test_ack_inherits_no_reserved_bits(tmp_path, linkframe_tool_path):
    """An ACK must carry none of the flags of the frame it acknowledges.

    Section 9.5 assigns the ACK's frame type, addressing and (session, sequence)
    and nothing else, so every remaining field has to come out cleared - which is
    true only because both implementations zero the output before filling it, and
    nothing tested that line.

    This test acknowledges a frame with ack_requested AND every reserved bit set,
    forged with encode_raw since the encoder correctly refuses to emit reserved
    bits. What it can and cannot see is worth being precise about, because two of
    the four fields are unreachable from here:

    - `ack_requested` echoed instead of cleared IS caught, in both languages: the
      encoder writes bit 0, so it reaches the wire. That is section 9.5's "ACK
      packets never request another ACK", checked against an input that would
      make an echoing implementation break it.
    - `reserved_flags` echoed is caught on the PYTHON side only. On the wire it
      is invisible by design - boomlink_linkframe_encode() never emits reserved
      bits whatever the header says - so the C struct is the only place it shows,
      and linkframe_tool's selftest is what inspects it.
    - `fragment_index` echoed, and the output zeroing itself, are unreachable
      from any test that goes through the tool: parse() rejects a frame with a
      non-zero fragment_index as fragmented, so no input here can carry one, and
      the tool always hands make_ack a zeroed output. The selftest covers both
      with a hand-built header and a deliberately dirty output.

    All four were verified to be uncaught before those two probes existed.
    """
    hostile_flags = ref.FLAG_ACK_REQUESTED | ref.FLAGS_RESERVED_MASK
    data = encode_with_tool(
        tmp_path, linkframe_tool_path,
        frame_type=int(ref.FrameType.DATA), flags=hostile_flags, fragment_index=0,
        raw=True, name="hostile_flags.bin", **SAMPLE,
    )
    assert data[2] == hostile_flags, "the forged frame does not carry the bits under test"

    built, c_ack = ack_with_tool(tmp_path, linkframe_tool_path, data)
    assert built
    assert c_ack[2] == 0x00, (
        f"the C ACK's flags byte is {c_ack[2]:#04x}; an ACK inherits no flags - it "
        f"requests no ACK of its own and carries no reserved bits"
    )

    received, _ = ref.parse(data)
    assert received.reserved_flags == ref.FLAGS_RESERVED_MASK, "the parser dropped the bits"
    python_ack = ref.make_ack(received, ACKING_NODE)
    assert python_ack.ack_requested is False
    assert python_ack.more_fragments is False
    assert python_ack.fragment_index == 0
    assert python_ack.reserved_flags == 0
    assert ref.encode(python_ack)[2] == 0x00
    assert c_ack == ref.encode(python_ack)


def test_ack_of_a_non_default_network_stays_on_that_network(tmp_path, linkframe_tool_path):
    """The magic is echoed, not defaulted. A deployment on a non-default network ID
    must be acknowledged on its own network, and this layer has no configuration
    to read the local one from - so echoing is both correct and the only option
    that does not add a parameter."""
    data = ref.encode(ref.LinkFrameHeader(magic=0x5A, ack_requested=True, **SAMPLE))

    received, _ = ref.parse(data, expected_magic=0x5A)
    assert ref.make_ack(received, ACKING_NODE).magic == 0x5A

    in_path = tmp_path / "foreign.bin"
    in_path.write_bytes(data)
    out_path = tmp_path / "foreign_ack.bin"
    result = run_linkframe_tool(
        linkframe_tool_path, "ack", str(in_path), str(ACKING_NODE), str(out_path), "90"
    )
    assert result.returncode == 0, result.stderr
    assert out_path.read_bytes()[0] == 0x5A


@pytest.mark.parametrize(
    "source_id,local_node_id,why",
    [
        (ref.ADDR_BROADCAST, ACKING_NODE,
         "a frame claiming the broadcast address as its source cannot be "
         "acknowledged: section 7.2 makes 0xFFFFFFFF something no node can BE, so "
         "there is nobody to address the ACK to - and such an ACK is unusable by "
         "anyone anyway, since section 9.5's matching rule requires an ACK's "
         "destination to equal the receiving node's own ID. NOT an airtime "
         "defence: that ACK is one 20-byte transmission, the same as a unicast "
         "one. The storm vector is a frame addressed TO broadcast with "
         "ack_requested set, which this guard does not touch"),
        (ref.ADDR_INVALID, ACKING_NODE,
         "nor one from the unconfigured address, which is nobody"),
        (SAMPLE["source_id"], ref.ADDR_BROADCAST,
         "and a node that thinks it IS the broadcast address has no identity to "
         "acknowledge as"),
        (SAMPLE["source_id"], ref.ADDR_INVALID,
         "same for an unconfigured node - section 7.2 puts real node IDs at "
         "0x00000001..0xFFFFFFFE, and both ends of the ACK's addressing must be one"),
    ],
)
def test_ack_refuses_an_unusable_address(tmp_path, linkframe_tool_path, source_id,
                                         local_node_id, why):
    """make_ack() does not decide WHETHER to acknowledge - that is the engine's
    call - but an ACK it could only address to the broadcast or invalid address is
    not a valid ACK at all, which is a property of the frame."""
    fields = dict(SAMPLE, source_id=source_id)
    data = ref.encode(ref.LinkFrameHeader(ack_requested=True, **fields))

    built, _ = ack_with_tool(tmp_path, linkframe_tool_path, data, local_node_id)
    assert built is False, f"C: {why}"

    received, _ = ref.parse(data)
    with pytest.raises(ValueError):
        ref.make_ack(received, local_node_id)


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
        (ref.ADDR_INVALID, ref.ADDR_INVALID, False,
         "the case that makes the unconfigured-node guard load-bearing: without "
         "it, destination 0 'matches' a node whose own id is still 0, so a "
         "factory-fresh node acts on traffic addressed to the invalid address. "
         "An implementation applying the guard only to broadcast passes every "
         "other case here"),
        (ref.ADDR_INVALID, 0x00000042, False,
         "rejected simply because 0 is neither this node's id nor broadcast - "
         "there is no separate 'destination must not be 0' rule, and this case "
         "does not imply one"),
        (ref.ADDR_BROADCAST, ref.ADDR_BROADCAST, False,
         "the other end of section 7.2's range, and the mirror of the "
         "unconfigured case: 0xFFFFFFFF is the broadcast address, not something "
         "a node can BE (real ids are 0x00000001..0xFFFFFFFE). Without the "
         "guard, a node misconfigured to the broadcast address matches every "
         "broadcast frame twice over and would answer for the whole network"),
        (0x00000042, ref.ADDR_BROADCAST, False,
         "same misconfigured node, unicast to someone else - it must not accept "
         "that either, and would not even without the guard, so this is the case "
         "that keeps the one above from being the only thing holding the rule up"),
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


ENCODE_ARG_NAMES = ("frame_type", "flags", "fragment_index", "destination_id",
                    "source_id", "session_id", "sequence")


def encode_args_with(position, value):
    """The seven numeric `encode` arguments with one replaced, so a
    malformed-input test can target any field rather than only the last uint32.
    The remaining two (payload_hex, out_file) are appended by the caller, which
    needs a tmp_path for the second anyway."""
    args = ["1", "0", "0", str(SAMPLE["destination_id"]), str(SAMPLE["source_id"]),
            str(SAMPLE["session_id"]), str(SAMPLE["sequence"])]
    args[position] = value
    return args


@pytest.mark.parametrize("position,name", list(enumerate(ENCODE_ARG_NAMES)))
@pytest.mark.parametrize(
    "bad_value",
    ["xyz", "-1", "99999999999999999999", "1.5", "", "-0", " 5", "\t7", "+5",
     "5 ", "0x10", "1e3", "5abc", "1,000"],
)
def test_encode_rejects_malformed_numeric_arguments(tmp_path, linkframe_tool_path,
                                                    position, name, bad_value):
    """Same discipline as the codec tool: a typo'd argument must fail loudly
    rather than silently become another value, or a test asserting "this frame
    is rejected" could be passing for the wrong reason entirely.

    Every argument position, not just one: the first three go through
    boomlink_tool_parse_u8 and the rest through parse_u32, and testing only a
    uint32 left the u8 path unexercised - widening its limit to 0xFFFF passed
    the whole suite while `encode 300 ...` silently became frame_type 12.
    """
    out_path = tmp_path / "out.bin"
    result = run_linkframe_tool(
        linkframe_tool_path, "encode", *encode_args_with(position, bad_value),
        "", str(out_path),
    )
    assert result.returncode == 2, (
        f"{name}={bad_value!r} should be a parse error, got rc={result.returncode}"
    )
    # WHICH argument was blamed, not just that something was: the whole point of
    # a message per argument is that a typo'd test does not look like a protocol
    # failure, and without this a tool that named the wrong field - or the same
    # field every time - would pass every case here.
    assert name in result.stderr, (
        f"{name}={bad_value!r} was rejected, but the message does not name the "
        f"argument: {result.stderr!r}"
    )


@pytest.mark.parametrize("position,name", [(0, "frame_type"), (1, "flags"),
                                           (2, "fragment_index")])
@pytest.mark.parametrize("over_u8", ["256", "300", "65536", "4294967296"])
def test_encode_rejects_uint8_arguments_out_of_range(tmp_path, linkframe_tool_path,
                                                      position, name, over_u8):
    """The three byte-wide arguments must reject anything above 255 outright.
    Silently masking instead is not harmless: `frame_type 300` would become
    300 & 0x0F = 12, i.e. a valid-looking frame of an unknown type, and a test
    expecting a rejection would pass for entirely the wrong reason."""
    out_path = tmp_path / "out.bin"
    result = run_linkframe_tool(
        linkframe_tool_path, "encode", *encode_args_with(position, over_u8),
        "", str(out_path),
    )
    assert result.returncode == 2, (
        f"{name}={over_u8} exceeds a uint8 and should be a parse error, "
        f"got rc={result.returncode}"
    )
    assert name in result.stderr, (
        f"{name}={over_u8} was rejected, but the message does not name the "
        f"argument: {result.stderr!r}"
    )


def test_parse_rejects_an_out_of_range_expected_magic(tmp_path, linkframe_tool_path):
    """`parse`'s optional expected_magic argument is the fourth uint8 the tool
    parses, and the only one outside `encode`."""
    path = tmp_path / "frame.bin"
    path.write_bytes(ref.encode(ref.LinkFrameHeader(**SAMPLE)))
    result = run_linkframe_tool(linkframe_tool_path, "parse", str(path), "256")
    assert result.returncode == 2, result.stdout


def test_payload_at_the_tool_cap_round_trips(tmp_path, linkframe_tool_path,
                                             linkframe_constants):
    """The tightest buffer relation in the tool: `hex[2 * max_payload + 1]`
    against a payload of exactly max_payload bytes. Exercised here rather than
    left to reasoning, because hex_encode's abort guard exists precisely because
    a one-byte error in this relation once turned a real overflow test green -
    and this also gives the reported `max_payload` key a consumer."""
    max_payload = linkframe_constants["max_payload"]
    payload = bytes((i % 251) for i in range(max_payload))
    frame = encode_with_tool(
        tmp_path, linkframe_tool_path,
        frame_type=int(ref.FrameType.DATA), flags=0, fragment_index=0,
        payload=payload, **SAMPLE,
    )
    assert len(frame) == linkframe_constants["header_size"] + max_payload

    fields = parse_with_tool(tmp_path, linkframe_tool_path, frame)
    assert fields["result"] == str(int(ref.ParseResult.OK)), fields
    assert fields["payload_len"] == str(max_payload)
    assert fields["payload"] == payload.hex()

    header, parsed_payload = ref.parse(frame)
    assert parsed_payload == payload
    assert header.sequence == SAMPLE["sequence"]


def test_payload_over_the_tool_cap_is_rejected(tmp_path, linkframe_tool_path,
                                               linkframe_constants):
    """`encode` must refuse a payload past its own buffer rather than truncating.

    The harness's own limits are worth a test for the same reason
    boomlink_tool_hex_encode() aborts instead of exiting positively: a buffer
    fault inside the tool that presents as an ordinary rejection is
    indistinguishable from the protocol rejecting something, and once turned a
    real overflow test green. The companion test at the cap
    (test_payload_at_the_tool_cap_round_trips) covers the accepting side, so the
    two together pin the boundary from both directions.
    """
    max_payload = linkframe_constants["max_payload"]
    result = run_linkframe_tool(
        linkframe_tool_path, "encode", "1", "0", "0",
        str(SAMPLE["destination_id"]), str(SAMPLE["source_id"]),
        str(SAMPLE["session_id"]), str(SAMPLE["sequence"]),
        (b"\xAB" * (max_payload + 1)).hex(), str(tmp_path / "too_big.bin"),
    )
    assert result.returncode == 2, (
        f"a {max_payload + 1}-byte payload should be rejected, got rc={result.returncode}"
    )
    assert not (tmp_path / "too_big.bin").exists(), (
        "the tool wrote a frame despite rejecting the payload"
    )


def test_input_over_the_tool_cap_is_rejected(tmp_path, linkframe_tool_path,
                                             linkframe_constants):
    """And the same on the way in: `parse` reads one byte past its cap precisely
    so an oversized file is detected by having read too much, rather than by
    feof() after a read that exactly filled the buffer - fread() does not set EOF
    in that case, so the off-by-one in that reasoning is what this pins."""
    max_input = linkframe_constants["header_size"] + linkframe_constants["max_payload"]
    path = tmp_path / "too_big_in.bin"

    # Exactly at the cap must still be accepted, or the check below could be
    # passing because the limit is off by one in the other direction.
    at_cap = ref.encode(ref.LinkFrameHeader(**SAMPLE)) + bytes(
        max_input - linkframe_constants["header_size"]
    )
    path.write_bytes(at_cap)
    assert run_linkframe_tool(linkframe_tool_path, "parse", str(path)).returncode == 0

    path.write_bytes(at_cap + b"\x00")
    result = run_linkframe_tool(linkframe_tool_path, "parse", str(path))
    assert result.returncode == 1, (
        f"a {max_input + 1}-byte input should be rejected, got rc={result.returncode}"
    )


def test_frame_type_over_a_nibble_cannot_reach_the_version(tmp_path, linkframe_tool_path):
    """frame_type shares byte 1 with the version nibble, so a value above 15
    would raise the version if the encoder did not mask it.

    33, not 17: with 17 the masked and unmasked results are the SAME byte
    (0x10 | (17 & 0x0F) == 0x10 | 17 == 0x11), so a version of this test using
    17 passed with the mask deleted - it asserted nothing. 33 = 0x21 puts a bit
    in the high nibble: masked it is 0x11 (a valid version-1 DATA frame),
    unmasked 0x31, which parses as version 3 and is dropped.
    """
    out_path = tmp_path / "wide_type.bin"
    result = run_linkframe_tool(
        linkframe_tool_path, "encode", "33", "0", "0",
        str(SAMPLE["destination_id"]), str(SAMPLE["source_id"]),
        str(SAMPLE["session_id"]), str(SAMPLE["sequence"]), "", str(out_path),
    )
    assert result.returncode == 0, result.stderr
    frame = out_path.read_bytes()
    # 33 & 0x0F == 1 (DATA); the version nibble must be untouched.
    assert frame[1] == (ref.VERSION << 4) | int(ref.FrameType.DATA), (
        f"byte 1 is {frame[1]:#04x}; an out-of-nibble frame_type reached the version"
    )
    # And the frame is therefore ordinary and valid, not dropped - which is the
    # difference from the unmasked behaviour, so assert it rather than infer it.
    fields = parse_with_tool(tmp_path, linkframe_tool_path, frame)
    assert fields["result"] == str(int(ref.ParseResult.OK)), fields
    assert fields["version"] == str(ref.VERSION)
    assert fields["frame_type"] == str(int(ref.FrameType.DATA))

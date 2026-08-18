"""Protocol compatibility rules (boomlink.md section 15.1):

- maximum bounded field sizes;
- malformed/truncated input;
- unknown fields for forward compatibility;
- old golden vectors decoding with the current schema, and never silently
  replaced.

Encode/decode round-trips and the two encode-A/decode-B interop directions
live in test_encode_decode.py; this file is specifically the regression
guard against breaking those rules.
"""

import envelope_pb2
import pytest
from _support import BOOMLINK_PROTOCOL_VERSION, assert_clean_rejection, parse_kv, run_codec_tool
from vectors_spec import GOLDEN_VECTORS, VECTORS_DIR, verify_vector_hashes


def test_golden_vector_hashes_are_unchanged():
    """Catches a committed vector being silently replaced (e.g.
    `rm vectors/foo.bin && python generate_vectors.py` producing a fresh
    "historical" encoding) before the more permissive decode-based tests
    below even run - those would still pass against a replaced vector as
    long as it happens to decode to the same field values, which defeats
    the point of freezing historical bytes in the first place."""
    verify_vector_hashes()


def test_no_untracked_vector_files():
    """vectors_spec.py calls GOLDEN_VECTORS the single source of truth for
    tests/vectors/*.bin, but that is only true if every file actually in
    that directory has an entry - otherwise a stray, renamed, or
    accidentally-committed .bin file sits there completely untested,
    silently, forever."""
    on_disk = {path.name for path in VECTORS_DIR.glob("*.bin")}
    assert on_disk == set(GOLDEN_VECTORS), (
        f"tests/vectors/*.bin does not match vectors_spec.py's GOLDEN_VECTORS - "
        f"on disk but not in the spec: {on_disk - set(GOLDEN_VECTORS)}; "
        f"in the spec but missing on disk: {set(GOLDEN_VECTORS) - on_disk}"
    )


@pytest.mark.parametrize("filename,expected", GOLDEN_VECTORS.items())
def test_golden_vector_decodes_with_python(filename, expected):
    envelope = envelope_pb2.Envelope()
    envelope.ParseFromString((VECTORS_DIR / filename).read_bytes())
    assert envelope.header.protocol_version == expected["protocol_version"]
    assert envelope.header.request_id == expected["request_id"]
    assert envelope.system.WhichOneof("message") == expected["kind"]
    assert getattr(envelope.system, expected["kind"]).payload == expected["payload"]


@pytest.mark.parametrize("filename,expected", GOLDEN_VECTORS.items())
def test_golden_vector_decodes_with_nanopb(codec_tool_path, filename, expected):
    result = run_codec_tool(codec_tool_path, "decode", str(VECTORS_DIR / filename))
    assert result.returncode == 0, result.stderr
    fields = parse_kv(result.stdout)
    assert fields["kind"] == expected["kind"]
    assert fields["protocol_version"] == str(expected["protocol_version"])
    assert fields["request_id"] == str(expected["request_id"])
    assert fields["payload"] == expected["payload"].hex()


def test_max_bounded_field_size_is_accepted(tmp_path, codec_tool_path, codec_tool_limits):
    """Exactly the compiled bound must round-trip - the bound is inclusive,
    not exclusive."""
    max_size = codec_tool_limits["ping_payload_max"]
    envelope = envelope_pb2.Envelope()
    envelope.header.protocol_version = BOOMLINK_PROTOCOL_VERSION
    envelope.system.ping.payload = b"\x01" * max_size
    path = tmp_path / "envelope.bin"
    path.write_bytes(envelope.SerializeToString())

    result = run_codec_tool(codec_tool_path, "decode", str(path))
    assert result.returncode == 0, result.stderr
    assert parse_kv(result.stdout)["payload"] == ("01" * max_size)


def test_field_size_one_over_bound_is_rejected(tmp_path, codec_tool_path, codec_tool_limits):
    envelope = envelope_pb2.Envelope()
    envelope.header.protocol_version = BOOMLINK_PROTOCOL_VERSION
    envelope.system.ping.payload = b"\x01" * (codec_tool_limits["ping_payload_max"] + 1)
    path = tmp_path / "envelope.bin"
    path.write_bytes(envelope.SerializeToString())

    result = run_codec_tool(codec_tool_path, "decode", str(path))
    assert_clean_rejection(result)


def test_malformed_input_is_rejected(tmp_path, codec_tool_path):
    path = tmp_path / "garbage.bin"
    path.write_bytes(b"\xff\xff\xff\xff\xff\xff\xff\xff")

    result = run_codec_tool(codec_tool_path, "decode", str(path))
    assert_clean_rejection(result)


def test_truncated_input_is_rejected(tmp_path, codec_tool_path):
    envelope = envelope_pb2.Envelope()
    envelope.header.protocol_version = BOOMLINK_PROTOCOL_VERSION
    envelope.system.ping.payload = b"\xde\xad\xbe\xef"
    full = envelope.SerializeToString()
    assert len(full) > 1

    path = tmp_path / "truncated.bin"
    path.write_bytes(full[:-1])

    result = run_codec_tool(codec_tool_path, "decode", str(path))
    assert_clean_rejection(result)


def test_missing_header_is_rejected(tmp_path, codec_tool_path):
    """A wire-valid Envelope with no `header` submessage at all is malformed
    at the application level (boomlink.md section 7 has every message carry
    one) - boomlink_codec.c must reject it, not hand back a decoded value
    with a meaningless zeroed header."""
    envelope = envelope_pb2.Envelope()
    envelope.system.ping.payload = b"\xde\xad\xbe\xef"  # header untouched
    assert not envelope.HasField("header")

    path = tmp_path / "envelope.bin"
    path.write_bytes(envelope.SerializeToString())

    result = run_codec_tool(codec_tool_path, "decode", str(path))
    assert_clean_rejection(result)


def test_zero_protocol_version_is_rejected(tmp_path, codec_tool_path):
    """protocol_version starts at 1 (header.proto); 0 is proto3's default for
    an omitted scalar, i.e. "never actually set" - accepting it would treat a
    caller's mistake as protocol version 0. Checked on both the decode side
    (a peer sent it) and the encode side (this codec would produce it)."""
    envelope = envelope_pb2.Envelope()
    envelope.header.request_id = 1  # marks `header` present; protocol_version stays 0
    assert envelope.HasField("header")
    assert envelope.header.protocol_version == 0

    path = tmp_path / "envelope.bin"
    path.write_bytes(envelope.SerializeToString())
    decode_result = run_codec_tool(codec_tool_path, "decode", str(path))
    assert_clean_rejection(decode_result)

    encode_out = tmp_path / "encoded.bin"
    encode_result = run_codec_tool(codec_tool_path, "encode", "ping", "0", "1", "", str(encode_out))
    assert_clean_rejection(encode_result)


def test_encode_rejects_malformed_numeric_arguments(tmp_path, codec_tool_path):
    """A typo'd CLI argument must fail loudly, not silently become some
    other value - otherwise a test asserting "protocol_version 0 is
    rejected" (above) would pass identically for a typo'd argument instead
    of the value it claims to be testing. Includes cases a first version of
    this parser accepted by accident: `strtoul`/`strtoull` alone parse a
    leading '-' as a negated-then-wrapped unsigned value rather than
    rejecting it (so "-1" only failed here because the wrapped result
    happened to still be out of uint32 range on a 64-bit build - a
    genuinely portable check must reject the '-' itself), plus leading
    whitespace and a leading '+', which `strtoul` also accepts silently."""
    out_path = tmp_path / "out.bin"
    bad_protocol_versions = (
        "xyz", "-1", "99999999999999999999", "1.5", "",
        "-0", " 5", "\t7", "+5", "5 ", "0x10", "1e3", "5abc", "1,000",
    )
    for bad_protocol_version in bad_protocol_versions:
        result = run_codec_tool(codec_tool_path, "encode", "ping", bad_protocol_version, "1", "",
                                 str(out_path))
        assert result.returncode == 2, (
            f"protocol_version={bad_protocol_version!r} should be a parse error, "
            f"got rc={result.returncode}"
        )


def test_decode_read_cap_boundary(tmp_path, codec_tool_path, codec_tool_limits):
    """codec_tool's own read cap (BOOMLINK_DECODE_READ_CAP - generously above
    the real on-air budget, see codec_tool.c's decode comment) must accept
    exactly `decode_read_cap` bytes and reject only what's strictly larger,
    with a distinct "too large" rejection rather than misreporting an
    oversized input as an ordinary decode failure. A previous version of
    this cap had an off-by-one here (rejecting a legitimately-sized input of
    precisely the cap's own size) that this test guards against
    regressing - garbage bytes are used rather than a real Envelope encoding
    since only the read stage's own boundary behaviour is under test here,
    not boomlink_decode_envelope() itself."""
    read_cap = codec_tool_limits["decode_read_cap"]

    at_cap = tmp_path / "at_cap.bin"
    at_cap.write_bytes(b"\xff" * read_cap)
    result_at_cap = run_codec_tool(codec_tool_path, "decode", str(at_cap))
    assert_clean_rejection(result_at_cap)
    assert "exceeds the maximum test input size" not in result_at_cap.stderr, (
        f"a {read_cap}-byte input (exactly the read cap) was rejected as too "
        f"large:\n{result_at_cap.stderr}"
    )

    over_cap = tmp_path / "over_cap.bin"
    over_cap.write_bytes(b"\xff" * (read_cap + 1))
    result_over_cap = run_codec_tool(codec_tool_path, "decode", str(over_cap))
    assert_clean_rejection(result_over_cap)
    assert "exceeds the maximum test input size" in result_over_cap.stderr, (
        f"a {read_cap + 1}-byte input (one over the read cap) was not rejected "
        f"as too large:\n{result_over_cap.stderr}"
    )


def test_unknown_field_is_forward_compatible(tmp_path, codec_tool_path):
    """A peer running a newer schema (a hypothetical future Envelope field)
    must not break an older decoder - the field is skipped, not an error, on
    both the Python and the Nanopb side."""
    envelope = envelope_pb2.Envelope()
    envelope.header.protocol_version = BOOMLINK_PROTOCOL_VERSION
    envelope.system.ping.payload = b"\xde\xad\xbe\xef"
    base = envelope.SerializeToString()

    # Hand-appended field 20, wire type 2 (length-delimited): tag varint
    # 0xA2 0x01 ((20 << 3) | 2 = 162), length 3, payload b"\x01\x02\x03".
    # Field 20 is not assigned to anything in envelope.proto (10-13 are
    # reserved for PR 4's message groups, 14 is `system`) so it is
    # unambiguously "a field from a future schema" to both decoders.
    unknown_field = bytes([0xA2, 0x01, 0x03, 0x01, 0x02, 0x03])
    with_unknown_field = base + unknown_field

    reparsed = envelope_pb2.Envelope()
    reparsed.ParseFromString(with_unknown_field)
    assert reparsed.system.ping.payload == b"\xde\xad\xbe\xef"

    path = tmp_path / "envelope_with_unknown_field.bin"
    path.write_bytes(with_unknown_field)
    result = run_codec_tool(codec_tool_path, "decode", str(path))
    assert result.returncode == 0, result.stderr
    assert parse_kv(result.stdout)["payload"] == "deadbeef"


def test_unknown_field_is_forward_compatible_near_the_real_budget(
    tmp_path, codec_tool_path, codec_tool_limits
):
    """Same as test_unknown_field_is_forward_compatible, but sized close to
    the real on-air budget (codec_tool_limits["envelope_budget"] -
    BOOMLINK_RADIO_MAX_PAYLOAD minus the link frame header) rather than a
    few bytes over this schema's own worst case
    (codec_tool_limits["envelope_size"]). A previous version of codec_tool's
    `decode` sized its read buffer to exactly boomlink_Envelope_size and
    rejected anything larger as "too large" - which made it impossible to
    exercise this exact compatibility rule (section 15.1) at a realistic
    size: any legitimately-larger frame from a newer peer was rejected by
    the test tool itself, before boomlink_decode_envelope() ever got to
    prove it handles the extra field correctly.

    Both bounds are read from `codec_tool limits` rather than hardcoded -
    a hardcoded copy of a compiled bound is exactly the failure mode
    README.md warns against (it's how this package once shipped a real
    stack overflow when a bound changed and one copy wasn't updated)."""
    max_size = codec_tool_limits["ping_payload_max"]
    envelope_size = codec_tool_limits["envelope_size"]
    envelope_budget = codec_tool_limits["envelope_budget"]
    envelope = envelope_pb2.Envelope()
    envelope.header.protocol_version = BOOMLINK_PROTOCOL_VERSION
    envelope.system.ping.payload = b"\x01" * max_size
    base = envelope.SerializeToString()

    # Field 20, wire type 2: tag varint (20 << 3) | 2 = 162, which needs TWO
    # bytes to encode (162 > 127, so its continuation bit is set) - encoded
    # below as 0xA2 0x01, same as test_unknown_field_is_forward_compatible
    # above. Sized to land the whole frame right at the real on-air budget:
    # past this schema's own worst case (envelope_size), but never above
    # what a real LoRa packet can carry (envelope_budget).
    tag_and_length_overhead = 3  # 2-byte tag + 1-byte length varint - see assert below
    unknown_len = envelope_budget - len(base) - tag_and_length_overhead
    assert 0 < unknown_len < 128, (
        "test assumes the unknown field's own length fits in a 1-byte varint - "
        f"got unknown_len={unknown_len} from envelope_budget={envelope_budget}, "
        f"len(base)={len(base)}; the compiled bounds no longer leave the margin "
        "this test was written to exercise"
    )
    unknown_payload = b"\x02" * unknown_len
    unknown_field = bytes([0xA2, 0x01, len(unknown_payload)]) + unknown_payload
    with_unknown_field = base + unknown_field
    assert len(with_unknown_field) > envelope_size  # would have tripped the old, too-tight read cap
    assert len(with_unknown_field) <= envelope_budget  # still within the real on-air budget

    reparsed = envelope_pb2.Envelope()
    reparsed.ParseFromString(with_unknown_field)
    assert reparsed.system.ping.payload == b"\x01" * max_size

    path = tmp_path / "envelope_with_large_unknown_field.bin"
    path.write_bytes(with_unknown_field)
    result = run_codec_tool(codec_tool_path, "decode", str(path))
    assert result.returncode == 0, result.stderr
    assert parse_kv(result.stdout)["payload"] == ("01" * max_size)

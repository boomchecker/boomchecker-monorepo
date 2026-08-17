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
from _support import parse_kv, run_codec_tool
from vectors_spec import GOLDEN_VECTORS, VECTORS_DIR, verify_vector_hashes


def test_golden_vector_hashes_are_unchanged():
    """Catches a committed vector being silently replaced (e.g.
    `rm vectors/foo.bin && python generate_vectors.py` producing a fresh
    "historical" encoding) before the more permissive decode-based tests
    below even run - those would still pass against a replaced vector as
    long as it happens to decode to the same field values, which defeats
    the point of freezing historical bytes in the first place."""
    verify_vector_hashes()


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
    envelope.header.protocol_version = 1
    envelope.system.ping.payload = b"\x01" * max_size
    path = tmp_path / "envelope.bin"
    path.write_bytes(envelope.SerializeToString())

    result = run_codec_tool(codec_tool_path, "decode", str(path))
    assert result.returncode == 0, result.stderr
    assert parse_kv(result.stdout)["payload"] == ("01" * max_size)


def test_field_size_one_over_bound_is_rejected(tmp_path, codec_tool_path, codec_tool_limits):
    envelope = envelope_pb2.Envelope()
    envelope.header.protocol_version = 1
    envelope.system.ping.payload = b"\x01" * (codec_tool_limits["ping_payload_max"] + 1)
    path = tmp_path / "envelope.bin"
    path.write_bytes(envelope.SerializeToString())

    result = run_codec_tool(codec_tool_path, "decode", str(path))
    assert result.returncode != 0


def test_malformed_input_is_rejected(tmp_path, codec_tool_path):
    path = tmp_path / "garbage.bin"
    path.write_bytes(b"\xff\xff\xff\xff\xff\xff\xff\xff")

    result = run_codec_tool(codec_tool_path, "decode", str(path))
    assert result.returncode != 0


def test_truncated_input_is_rejected(tmp_path, codec_tool_path):
    envelope = envelope_pb2.Envelope()
    envelope.header.protocol_version = 1
    envelope.system.ping.payload = b"\xde\xad\xbe\xef"
    full = envelope.SerializeToString()
    assert len(full) > 1

    path = tmp_path / "truncated.bin"
    path.write_bytes(full[:-1])

    result = run_codec_tool(codec_tool_path, "decode", str(path))
    assert result.returncode != 0


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
    assert result.returncode != 0


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
    assert decode_result.returncode != 0

    encode_out = tmp_path / "encoded.bin"
    encode_result = run_codec_tool(codec_tool_path, "encode", "ping", "0", "1", "", str(encode_out))
    assert encode_result.returncode != 0


def test_encode_rejects_malformed_numeric_arguments(tmp_path, codec_tool_path):
    """A typo'd CLI argument must fail loudly, not silently become some
    other value - otherwise a test asserting "protocol_version 0 is
    rejected" (above) would pass identically for a typo'd argument instead
    of the value it claims to be testing."""
    out_path = tmp_path / "out.bin"
    for bad_protocol_version in ("xyz", "-1", "99999999999999999999", "1.5", ""):
        result = run_codec_tool(codec_tool_path, "encode", "ping", bad_protocol_version, "1", "",
                                 str(out_path))
        assert result.returncode == 2, (
            f"protocol_version={bad_protocol_version!r} should be a parse error, "
            f"got rc={result.returncode}"
        )


def test_unknown_field_is_forward_compatible(tmp_path, codec_tool_path):
    """A peer running a newer schema (a hypothetical future Envelope field)
    must not break an older decoder - the field is skipped, not an error, on
    both the Python and the Nanopb side."""
    envelope = envelope_pb2.Envelope()
    envelope.header.protocol_version = 1
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

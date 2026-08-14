"""Protocol compatibility rules (boomlink.md section 15.1):

- maximum bounded field sizes;
- malformed/truncated input;
- unknown fields for forward compatibility;
- old golden vectors decoding with the current schema.

Encode/decode round-trips and the two encode-A/decode-B interop directions
live in test_encode_decode.py; this file is specifically the regression
guard against breaking those five rules.
"""

import pathlib
import subprocess

import envelope_pb2
import pytest

VECTORS_DIR = pathlib.Path(__file__).parent / "vectors"

# Never edit an existing entry here to match a "fixed" vector file - if an
# old vector stops decoding to these values, the schema broke backward
# compatibility, which is exactly what this test exists to catch. Add new
# vectors (and new entries) instead - see generate_vectors.py.
GOLDEN_VECTORS = {
    "ping_basic.bin": {"protocol_version": 1, "request_id": 1, "kind": "ping", "payload": b""},
    "ping_with_payload.bin": {
        "protocol_version": 1,
        "request_id": 2,
        "kind": "ping",
        "payload": bytes.fromhex("deadbeef"),
    },
    "pong_basic.bin": {"protocol_version": 1, "request_id": 3, "kind": "pong", "payload": b""},
    "pong_max_payload.bin": {
        "protocol_version": 1,
        "request_id": 4,
        "kind": "pong",
        "payload": b"\xaa" * 64,
    },
}


def _run(codec_tool_path, *args):
    return subprocess.run([codec_tool_path, *args], capture_output=True, text=True, timeout=10)


def _parse_kv(stdout):
    fields = {}
    for line in stdout.strip().splitlines():
        key, _, value = line.partition("=")
        fields[key] = value
    return fields


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
    result = _run(codec_tool_path, "decode", str(VECTORS_DIR / filename))
    assert result.returncode == 0, result.stderr
    fields = _parse_kv(result.stdout)
    assert fields["kind"] == expected["kind"]
    assert fields["protocol_version"] == str(expected["protocol_version"])
    assert fields["request_id"] == str(expected["request_id"])
    assert fields["payload"] == expected["payload"].hex()


def test_max_bounded_field_size_is_accepted(tmp_path, codec_tool_path):
    """Exactly nanopb/boomlink.options' max_size (64 bytes) must round-trip -
    the bound is inclusive, not exclusive."""
    envelope = envelope_pb2.Envelope()
    envelope.header.protocol_version = 1
    envelope.system.ping.payload = b"\x01" * 64
    path = tmp_path / "envelope.bin"
    path.write_bytes(envelope.SerializeToString())

    result = _run(codec_tool_path, "decode", str(path))
    assert result.returncode == 0, result.stderr
    assert _parse_kv(result.stdout)["payload"] == ("01" * 64)


def test_field_size_one_over_bound_is_rejected(tmp_path, codec_tool_path):
    envelope = envelope_pb2.Envelope()
    envelope.header.protocol_version = 1
    envelope.system.ping.payload = b"\x01" * 65
    path = tmp_path / "envelope.bin"
    path.write_bytes(envelope.SerializeToString())

    result = _run(codec_tool_path, "decode", str(path))
    assert result.returncode != 0


def test_malformed_input_is_rejected(tmp_path, codec_tool_path):
    path = tmp_path / "garbage.bin"
    path.write_bytes(b"\xff\xff\xff\xff\xff\xff\xff\xff")

    result = _run(codec_tool_path, "decode", str(path))
    assert result.returncode != 0


def test_truncated_input_is_rejected(tmp_path, codec_tool_path):
    envelope = envelope_pb2.Envelope()
    envelope.header.protocol_version = 1
    envelope.system.ping.payload = b"\xde\xad\xbe\xef"
    full = envelope.SerializeToString()
    assert len(full) > 1

    path = tmp_path / "truncated.bin"
    path.write_bytes(full[:-1])

    result = _run(codec_tool_path, "decode", str(path))
    assert result.returncode != 0


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
    result = _run(codec_tool_path, "decode", str(path))
    assert result.returncode == 0, result.stderr
    assert _parse_kv(result.stdout)["payload"] == "deadbeef"

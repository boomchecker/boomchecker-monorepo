"""Dynamic cross-language round-trip tests: Python Protobuf classes generated
straight from the .proto files (an independent, non-Nanopb implementation of
the same schema) versus boomlink_codec_tool, the compiled Nanopb side (see
test_encode_decode.c). boomlink.md section 15.1 requires both directions -
"Python Protobuf encode -> Nanopb decode" and "Nanopb encode -> Python
Protobuf decode" - to actually be exercised, not just each side tested in
isolation.
"""

import subprocess

import envelope_pb2
import pytest

# Matches nanopb/boomlink.options' Ping/Pong payload max_size.
MAX_PAYLOAD_SIZE = 192


def _run(codec_tool_path, *args):
    result = subprocess.run(
        [codec_tool_path, *args], capture_output=True, text=True, timeout=10
    )
    return result


def _parse_kv(stdout):
    fields = {}
    for line in stdout.strip().splitlines():
        key, _, value = line.partition("=")
        fields[key] = value
    return fields


def _make_ping_envelope(protocol_version, request_id, payload: bytes) -> envelope_pb2.Envelope:
    envelope = envelope_pb2.Envelope()
    envelope.header.protocol_version = protocol_version
    envelope.header.request_id = request_id
    envelope.system.ping.payload = payload
    return envelope


@pytest.mark.parametrize("payload", [b"", b"\xde\xad\xbe\xef", b"\x00" * MAX_PAYLOAD_SIZE])
def test_python_encode_nanopb_decode_ping(tmp_path, codec_tool_path, payload):
    envelope = _make_ping_envelope(protocol_version=1, request_id=7, payload=payload)
    encoded_path = tmp_path / "envelope.bin"
    encoded_path.write_bytes(envelope.SerializeToString())

    result = _run(codec_tool_path, "decode", str(encoded_path))
    assert result.returncode == 0, result.stderr
    fields = _parse_kv(result.stdout)

    assert fields["kind"] == "ping"
    assert fields["protocol_version"] == "1"
    assert fields["request_id"] == "7"
    assert fields["payload"] == payload.hex()


@pytest.mark.parametrize("kind", ["ping", "pong"])
def test_nanopb_encode_python_decode(tmp_path, codec_tool_path, kind):
    out_path = tmp_path / "envelope.bin"
    result = _run(codec_tool_path, "encode", kind, "2", "99", "cafe", str(out_path))
    assert result.returncode == 0, result.stderr

    envelope = envelope_pb2.Envelope()
    envelope.ParseFromString(out_path.read_bytes())

    assert envelope.header.protocol_version == 2
    assert envelope.header.request_id == 99
    assert envelope.WhichOneof("payload") == "system"
    assert envelope.system.WhichOneof("message") == kind
    payload = getattr(envelope.system, kind).payload
    assert payload == bytes.fromhex("cafe")


def test_nanopb_rejects_payload_over_bound(tmp_path, codec_tool_path):
    """Python protobuf's `bytes` fields have no built-in max-size - a peer
    running an older/different implementation could send more than Nanopb's
    compiled `nanopb/boomlink.options` bound (192 bytes) allows. Nanopb must
    fail closed instead of overflowing its fixed-size buffer."""
    oversized = b"\x41" * (MAX_PAYLOAD_SIZE + 8)
    envelope = _make_ping_envelope(protocol_version=1, request_id=1, payload=oversized)
    encoded_path = tmp_path / "envelope.bin"
    encoded_path.write_bytes(envelope.SerializeToString())

    result = _run(codec_tool_path, "decode", str(encoded_path))
    assert result.returncode != 0

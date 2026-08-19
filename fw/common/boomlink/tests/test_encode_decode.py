"""Dynamic cross-language round-trip tests: Python Protobuf classes generated
straight from the .proto files (an independent, non-Nanopb implementation of
the same schema) versus boomlink_codec_tool, the compiled Nanopb side (see
codec_tool.c). boomlink.md section 15.1 requires both directions -
"Python Protobuf encode -> Nanopb decode" and "Nanopb encode -> Python
Protobuf decode" - to actually be exercised, not just each side tested in
isolation.
"""

import os

import envelope_pb2
import pytest
from _support import (
    BOOMLINK_PROTOCOL_VERSION,
    assert_clean_rejection,
    parse_kv,
    query_codec_tool_limits,
    run_codec_tool,
)


def pytest_generate_tests(metafunc):
    """Parametrizes test_python_encode_nanopb_decode_ping's large-payload
    case from the tool's real compiled bound (queried directly here, since
    fixtures aren't available yet at collection time) instead of a hardcoded
    number that could drift out of sync with nanopb/system.options and
    turn "exercise a large payload" into "unintentionally exceed the real
    bound and fail for the wrong reason". By the time this hook runs,
    conftest.py's pytest_configure() has already verified BOOMLINK_CODEC_TOOL
    is set (or exited the session), so it's safe to read directly here."""
    if metafunc.function.__name__ == "test_python_encode_nanopb_decode_ping":
        limits = query_codec_tool_limits(os.environ["BOOMLINK_CODEC_TOOL"])
        metafunc.parametrize(
            "payload", [b"", b"\xde\xad\xbe\xef", b"\x00" * limits["ping_payload_max"]]
        )


def _make_ping_envelope(protocol_version, request_id, payload: bytes) -> envelope_pb2.Envelope:
    envelope = envelope_pb2.Envelope()
    envelope.header.protocol_version = protocol_version
    envelope.header.request_id = request_id
    envelope.system.ping.payload = payload
    return envelope


def test_python_encode_nanopb_decode_ping(tmp_path, codec_tool_path, payload):
    envelope = _make_ping_envelope(
        protocol_version=BOOMLINK_PROTOCOL_VERSION, request_id=7, payload=payload
    )
    encoded_path = tmp_path / "envelope.bin"
    encoded_path.write_bytes(envelope.SerializeToString())

    result = run_codec_tool(codec_tool_path, "decode", str(encoded_path))
    assert result.returncode == 0, result.stderr
    fields = parse_kv(result.stdout)

    assert fields["kind"] == "ping"
    assert fields["protocol_version"] == str(BOOMLINK_PROTOCOL_VERSION)
    assert fields["request_id"] == "7"
    assert fields["payload"] == payload.hex()


@pytest.mark.parametrize("kind", ["ping", "pong"])
def test_nanopb_encode_python_decode(tmp_path, codec_tool_path, kind):
    out_path = tmp_path / "envelope.bin"
    result = run_codec_tool(codec_tool_path, "encode", kind, "2", "99", "cafe", str(out_path))
    assert result.returncode == 0, result.stderr

    envelope = envelope_pb2.Envelope()
    envelope.ParseFromString(out_path.read_bytes())

    assert envelope.header.protocol_version == 2
    assert envelope.header.request_id == 99
    assert envelope.WhichOneof("payload") == "system"
    assert envelope.system.WhichOneof("message") == kind
    payload = getattr(envelope.system, kind).payload
    assert payload == bytes.fromhex("cafe")


def test_nanopb_rejects_payload_over_bound(tmp_path, codec_tool_path, codec_tool_limits):
    """Python protobuf's `bytes` fields have no built-in max-size - a peer
    running an older/different implementation could send more than Nanopb's
    compiled bound allows. Nanopb must fail closed instead of overflowing
    its fixed-size buffer."""
    oversized = b"\x41" * (codec_tool_limits["ping_payload_max"] + 8)
    envelope = _make_ping_envelope(
        protocol_version=BOOMLINK_PROTOCOL_VERSION, request_id=1, payload=oversized
    )
    encoded_path = tmp_path / "envelope.bin"
    encoded_path.write_bytes(envelope.SerializeToString())

    result = run_codec_tool(codec_tool_path, "decode", str(encoded_path))
    assert_clean_rejection(result)

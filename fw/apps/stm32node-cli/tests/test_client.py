"""DeviceClient behaviour over the in-memory transport."""

from __future__ import annotations

import pytest

from conftest import FakeTransport
from stm32node_cli.protocol.client import DeviceClient
from stm32node_cli.protocol.codec import ProtocolError, pack_header


def _pcm(nbytes: int) -> bytes:
    return bytes(range(256)) * (nbytes // 256) + bytes(range(nbytes % 256))


def test_version_reads_line_and_sends_command():
    t = FakeTransport(to_read=b"bom-stm32node CLI v0.1\r\n")
    client = DeviceClient(t)
    assert client.version() == "bom-stm32node CLI v0.1"
    assert t.written == b"version\n"


def test_start_stream_resyncs_and_reads_exact_payload():
    payload = _pcm(4096)
    # Junk (echoed command + prompt) precedes the header; client must resync.
    stream_bytes = b"stream 1\r\n> " + pack_header(len(payload)) + payload
    t = FakeTransport(to_read=stream_bytes)
    client = DeviceClient(t)

    handle = client.start_stream(1)
    assert t.written == b"stream 1\n"
    assert handle.header.byte_length == len(payload)

    received = handle.read_all()
    assert received == payload


def test_start_stream_stops_at_byte_length():
    payload = _pcm(2048)
    # Extra trailing bytes (e.g. a new prompt) must NOT be consumed as PCM.
    trailer = b"\r\n> "
    t = FakeTransport(to_read=pack_header(len(payload)) + payload + trailer)
    client = DeviceClient(t)

    handle = client.start_stream(1)
    assert handle.read_all() == payload
    # Trailer still readable from the transport afterwards.
    assert t.read(len(trailer)) == trailer


def test_start_stream_test_source_sends_streamtest():
    payload = _pcm(2048)
    t = FakeTransport(to_read=pack_header(len(payload)) + payload)
    client = DeviceClient(t)

    handle = client.start_stream(1, source="test")
    assert t.written == b"streamtest 1\n"
    assert handle.read_all() == payload


def test_start_stream_reads_block_aligned_length_from_header():
    # Firmware rounds up to whole 1024-sample blocks: 1 s -> 47 blocks (not 46.875).
    block_bytes = 1024 * 2
    byte_length = 47 * block_bytes  # 96256, i.e. > 1 * 48000 * 2 (96000)
    payload = _pcm(byte_length)
    t = FakeTransport(to_read=pack_header(byte_length) + payload)
    client = DeviceClient(t)

    handle = client.start_stream(1)
    assert handle.header.byte_length == byte_length
    assert len(handle.read_all()) == byte_length


def test_start_stream_rejects_nonpositive_seconds():
    client = DeviceClient(FakeTransport())
    with pytest.raises(ValueError):
        client.start_stream(0)


def test_start_stream_rejects_unknown_source():
    client = DeviceClient(FakeTransport())
    with pytest.raises(ValueError):
        client.start_stream(1, source="bogus")


def test_read_trailer_parses_health():
    t = FakeTransport(to_read=b"PCMEND overrun=1 err=0\n")
    trailer = DeviceClient(t).read_trailer()
    assert trailer is not None
    assert trailer.overrun is True
    assert trailer.err is False


def test_read_trailer_none_when_absent():
    # No trailer on the wire (e.g. aborted stream) -> None, no exception.
    assert DeviceClient(FakeTransport(to_read=b"")).read_trailer() is None


def test_resync_times_out_without_magic():
    t = FakeTransport(to_read=b"no magic here")
    client = DeviceClient(t)
    with pytest.raises(ProtocolError):
        # Bytes arrive but never the magic: wait out the (short) window, then fail.
        client.start_stream(1, ack_timeout=0.05)


def test_version_skips_echo_and_prompt():
    # embedded-cli echoes the command (wrapped in cursor escapes) behind a prompt,
    # then prints the answer. version() must return the answer, not the echo.
    echo = b"> \x1b[sversion\x1b[u\r\n"
    t = FakeTransport(to_read=echo + b"bom-stm32node CLI v0.1\r\n")
    assert DeviceClient(t).version() == "bom-stm32node CLI v0.1"
    assert t.written == b"version\n"


def test_start_stream_retries_then_fails_on_silence():
    # Total silence = command never landed -> resend each attempt, then give up.
    t = FakeTransport(to_read=b"")
    with pytest.raises(ProtocolError):
        DeviceClient(t).start_stream(1, retries=3, ack_timeout=0.05)
    assert t.written == b"stream 1\n" * 3


def test_start_stream_no_retry_when_response_present():
    # Echo precedes the header (command landed): the magic still arrives, so the
    # command is sent exactly once - no duplicate stream queued on the board.
    payload = _pcm(2048)
    t = FakeTransport(to_read=b"stream 1\r\n> " + pack_header(len(payload)) + payload)
    client = DeviceClient(t)
    client.start_stream(1, retries=3, ack_timeout=0.05)
    assert t.written == b"stream 1\n"


def test_start_stream_rejects_seconds_over_max():
    client = DeviceClient(FakeTransport())
    with pytest.raises(ValueError):
        client.start_stream(61)

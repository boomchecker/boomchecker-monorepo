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


def _live_autocomplete_echo(command: str) -> bytes:
    """Reproduce embedded-cli's live-autocompletion echo for a typed command.

    On each keystroke the board echoes the typed char and then, wrapped in
    cursor save/restore escapes, the remaining autocomplete suffix of the
    command (printLiveAutocompletion). With the escapes and CRs stripped these
    collapse to one line, e.g. "version" -> "versionersionrsion...".
    """
    raw = bytearray()
    for i, ch in enumerate(command):
        raw += ch.encode()
        raw += b"\x1b[s" + command[i + 1 :].encode() + b"\x1b[u"
    raw += b"\r\n"
    return bytes(raw)


def test_version_skips_live_autocompletion_echo():
    # Real hardware bug: with live autocompletion the echoed line is not equal to
    # the command ("versionersion rsion ..."), so an exact-match filter wrongly
    # returned it. version() must still find the real answer on the next line.
    echo = _live_autocomplete_echo("version")
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


# -- model / micslot -----------------------------------------------------------


def test_list_models_collects_lines_and_skips_echo():
    # Echoed command + prompt, then one line per model, then the next prompt (no
    # trailing newline). The listing has no trailer, so the client reads until the
    # transport goes quiet and keeps only the `model: ` lines.
    stream = (
        b"> model\r\n"
        b"model: mlp_f1   * feat=1..69 thr=8466\r\n"
        b"model: gbt_f1     feat=1..69 thr=2646\r\n"
        b"> "
    )
    t = FakeTransport(to_read=stream)
    assert DeviceClient(t).list_models() == [
        "model: mlp_f1   * feat=1..69 thr=8466",
        "model: gbt_f1     feat=1..69 thr=2646",
    ]
    assert t.written == b"model\n"


def test_list_models_empty_on_silence():
    assert DeviceClient(FakeTransport(to_read=b"")).list_models() == []


def test_select_model_returns_confirmation_and_sends_name():
    stream = b"> model gbt_f1\r\nmodel: gbt_f1 selected, default thr=2646 (not persisted)\r\n> "
    t = FakeTransport(to_read=stream)
    assert (
        DeviceClient(t).select_model("gbt_f1")
        == "model: gbt_f1 selected, default thr=2646 (not persisted)"
    )
    assert t.written == b"model gbt_f1\n"


def test_select_model_reports_unknown():
    t = FakeTransport(to_read=b"model: no such model 'nope'\r\n")
    assert DeviceClient(t).select_model("nope") == "model: no such model 'nope'"


def test_mic_slot_shows_current():
    t = FakeTransport(to_read=b"micslot: B (0x07F8)\r\n")
    assert DeviceClient(t).mic_slot() == "micslot: B (0x07F8)"
    assert t.written == b"micslot\n"


def test_select_mic_slot_returns_confirmation():
    t = FakeTransport(to_read=b"micslot: A selected (next detect/stream)\r\n")
    assert DeviceClient(t).select_mic_slot("a") == "micslot: A selected (next detect/stream)"
    assert t.written == b"micslot a\n"


class InterleavedTransport(FakeTransport):
    """FakeTransport that injects empty reads (transport timeouts) mid-stream.

    ``empties_at`` maps a read-call index to how many consecutive empty reads to
    return there, simulating a slow board pausing between (or before) its lines.
    """

    def __init__(self, to_read: bytes, empties_at: dict[int, int]) -> None:
        super().__init__(to_read)
        self._empties_at = dict(empties_at)
        self._calls = 0

    def read(self, size: int) -> bytes:
        pending = self._empties_at.get(self._calls, 0)
        if pending:
            self._empties_at[self._calls] = pending - 1
            return b""
        self._calls += 1
        return super().read(size)


def test_select_model_skips_autocomplete_echo_without_colon():
    # Live autocompletion collapses the echo into a line that starts with the
    # command word but has no colon ("modelgbt_f1odel"); the prefix filter must
    # skip it and return the real reply on the next line.
    stream = (
        b"> \x1b[smodel\x1b[u gbt_f1\r\n"
        b"modelgbt_f1odel\r\n"
        b"model: gbt_f1 selected, default thr=2646 (not persisted)\r\n"
    )
    t = FakeTransport(to_read=stream)
    assert (
        DeviceClient(t).select_model("gbt_f1")
        == "model: gbt_f1 selected, default thr=2646 (not persisted)"
    )


def test_select_model_survives_echo_lines_plus_silent_gap():
    # Echo lines must not count against the line budget together with a silent
    # gap: several non-matching lines, then one empty read (a 2s transport
    # timeout on a slow board), then the reply - it must still be returned.
    stream = (
        b"> model gbt_f1\r\n"
        b"modelgbt_f1odel\r\n"
        b"model: gbt_f1 selected, default thr=2646 (not persisted)\r\n"
    )
    # Read-call index is per _read_line byte read; inject the gap after the two
    # echo lines by counting their bytes.
    echo_bytes = len(b"> model gbt_f1\r\n") + len(b"modelgbt_f1odel\r\n")
    t = InterleavedTransport(stream, empties_at={echo_bytes: 1})
    assert (
        DeviceClient(t).select_model("gbt_f1")
        == "model: gbt_f1 selected, default thr=2646 (not persisted)"
    )


def test_read_prefixed_gives_up_after_consecutive_silence():
    # Two consecutive empty reads (board silent) end the wait with "".
    assert DeviceClient(FakeTransport(to_read=b"")).select_model("nope") == ""


def test_list_models_survives_slow_start():
    # Empty reads BEFORE the first line (board still turning the command around)
    # must not end the collection early.
    stream = (
        b"> model\r\n"
        b"model: mlp_f1   * feat=1..69 thr=8466\r\n"
        b"model: gbt_f1     feat=1..69 thr=2646\r\n"
    )
    t = InterleavedTransport(stream, empties_at={0: 3})
    assert DeviceClient(t).list_models() == [
        "model: mlp_f1   * feat=1..69 thr=8466",
        "model: gbt_f1     feat=1..69 thr=2646",
    ]

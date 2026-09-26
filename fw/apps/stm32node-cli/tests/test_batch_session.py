"""BatchRecordSession cuts one or more streams into equal WAV files (fake transport)."""

from __future__ import annotations

import csv
import wave

import pytest

from conftest import FakeTransport
from stm32node_cli.protocol.client import DeviceClient
from stm32node_cli.protocol.codec import StreamAborted, pack_header
from stm32node_cli.protocol.spec import CHANNELS, SAMPLE_RATE_HZ, SAMPLE_WIDTH_BYTES
from stm32node_cli.sessions.batch import BatchRecordSession, plan_streams
from stm32node_cli.transport.base import TransportTimeout

BYTES_PER_SECOND = SAMPLE_RATE_HZ * CHANNELS * SAMPLE_WIDTH_BYTES


def _stream(seconds: int, fill: int = 0, *, trailer: bytes = b"PCMEND overrun=0 err=0\n") -> bytes:
    """One complete PCM1 transfer: header, ``seconds`` of payload, trailer."""
    payload = bytes([fill % 256]) * (BYTES_PER_SECOND * seconds)
    return pack_header(len(payload)) + payload + trailer


def _read_pcm(path) -> bytes:
    with wave.open(str(path), "rb") as wf:
        assert wf.getframerate() == SAMPLE_RATE_HZ
        return wf.readframes(wf.getnframes())


def test_plan_packs_whole_chunks_into_the_longest_stream():
    assert plan_streams(10, 20, max_stream_s=60) == [6, 6, 6, 2]
    assert plan_streams(60, 3, max_stream_s=60) == [1, 1, 1]
    assert plan_streams(7, 3, max_stream_s=60) == [3]


def test_plan_rejects_chunks_longer_than_a_stream():
    with pytest.raises(ValueError):
        plan_streams(61, 1, max_stream_s=60)
    with pytest.raises(ValueError):
        plan_streams(10, 0)


def test_one_stream_is_cut_into_count_files(tmp_path):
    # 3 x 1 s fits one stream: the board is asked for 3 s once.
    t = FakeTransport(to_read=_stream(3))
    session = BatchRecordSession(DeviceClient(t), tmp_path)

    result = session.record(1, 3)

    assert t.written == b"stream 3\n"
    assert result.complete and result.streams == 1
    names = [c.path.name for c in result.chunks]
    assert names == ["chunk-001.wav", "chunk-002.wav", "chunk-003.wav"]
    for chunk in result.chunks:
        assert chunk.path.parent == result.directory
        assert len(_read_pcm(chunk.path)) == BYTES_PER_SECOND
        assert chunk.trailer is not None and not chunk.trailer.overrun


def test_chunks_are_cut_in_order_across_streams(tmp_path):
    # max_stream_s=2 forces 3 x 1 s into two streams (2 s + 1 s); each stream's
    # payload has its own fill byte so the cut can be checked, not just counted.
    t = FakeTransport(to_read=_stream(2, fill=0xAA) + _stream(1, fill=0x55))
    session = BatchRecordSession(DeviceClient(t), tmp_path, max_stream_s=2)

    result = session.record(1, 3)

    assert t.written == b"stream 2\nstream 1\n"
    assert result.streams == 2
    assert [c.stream for c in result.chunks] == [1, 1, 2]
    assert _read_pcm(result.chunks[0].path)[:4] == b"\xaa" * 4
    assert _read_pcm(result.chunks[2].path)[:4] == b"\x55" * 4


def test_files_are_written_as_they_fill(tmp_path):
    t = FakeTransport(to_read=_stream(2))
    session = BatchRecordSession(DeviceClient(t), tmp_path)

    seen: list[tuple[int, int, bool]] = []
    session.record(1, 2, on_chunk=lambda i, n, p: seen.append((i, n, p.exists())))

    assert seen == [(1, 2, True), (2, 2, True)]


def test_index_lists_every_chunk_with_health(tmp_path):
    t = FakeTransport(to_read=_stream(2, trailer=b"PCMEND overrun=1 err=0\n"))
    session = BatchRecordSession(DeviceClient(t), tmp_path)

    result = session.record(1, 2)

    with result.index_path.open(newline="") as fh:
        rows = list(csv.DictReader(fh))
    assert [r["file"] for r in rows] == ["chunk-001.wav", "chunk-002.wav"]
    assert {r["seconds"] for r in rows} == {"1.000"}
    assert {r["overrun"] for r in rows} == {"1"}
    assert {r["err"] for r in rows} == {"0"}


def test_abort_keeps_finished_files(tmp_path):
    # A 3 s stream is under way and the user presses q just after the first
    # second has arrived. Chunk 1 must already be on disk and in the index; the
    # few milliseconds received past it are not a recording and are dropped.
    header = pack_header(BYTES_PER_SECOND * 3)
    t = FakeTransport(to_read=header + b"\x01" * (BYTES_PER_SECOND * 3))
    session = BatchRecordSession(DeviceClient(t), tmp_path)

    # The client polls this before every read; fire once >= 1 s has been consumed.
    def should_abort() -> bool:
        return len(t._rx) <= BYTES_PER_SECOND * 2

    with pytest.raises(StreamAborted):
        session.record(1, 3, should_abort=should_abort)

    saved = sorted(p.name for p in tmp_path.rglob("chunk-*.wav"))
    assert saved == ["chunk-001.wav"]
    (index,) = tmp_path.rglob("index.csv")
    with index.open(newline="") as fh:
        assert [r["file"] for r in csv.DictReader(fh)] == ["chunk-001.wav"]


def test_long_tail_is_kept_when_the_board_stops_sending(tmp_path):
    # Two 2 s chunks were asked for (one 4 s stream) but the board goes quiet
    # after 3.5 s. The failure must reach the caller, but the 1.5 s past chunk 1
    # is still audio: kept as a short file whose real length the index shows
    # (minus the partial 2048-byte read the client's timeout discards).
    header = pack_header(BYTES_PER_SECOND * 4)
    t = FakeTransport(to_read=header + b"\x02" * (BYTES_PER_SECOND * 7 // 2))
    session = BatchRecordSession(DeviceClient(t), tmp_path)

    with pytest.raises(TransportTimeout):
        session.record(2, 2)

    (index,) = tmp_path.rglob("index.csv")
    with index.open(newline="") as fh:
        rows = list(csv.DictReader(fh))
    assert [r["file"] for r in rows] == ["chunk-001.wav", "chunk-002.wav"]
    assert rows[0]["seconds"] == "2.000"
    assert 1.4 < float(rows[1]["seconds"]) < 1.5
    assert [r["overrun"] for r in rows] == ["", ""]  # no trailer arrived
    tail = _read_pcm(index.parent / "chunk-002.wav")
    assert BYTES_PER_SECOND < len(tail) < BYTES_PER_SECOND * 3 // 2

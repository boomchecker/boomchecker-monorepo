"""Batch recording: many equal-length WAV files from one sitting.

``recording 10 20`` records twenty ten-second files. Each file is written the
moment its ten seconds have arrived, while the board is still streaming the
next ones, so an abort or a cable pulled halfway leaves every finished chunk on
disk and an ``index.csv`` beside them.

Why not twenty ``record 10`` calls: every ``stream`` starts with a PDM settling
transient (~0.12 s clipped) and a command round-trip, so back-to-back single
recordings lose a slice of audio at every boundary. Here the board is asked for
the longest stream that holds a whole number of chunks (60 s = six 10-second
chunks) and the host cuts it as it arrives, so boundaries inside one stream are
gapless and the transient hits only the first chunk of each stream.
"""

from __future__ import annotations

import csv
from collections.abc import Callable
from dataclasses import dataclass, field
from datetime import datetime
from pathlib import Path

from ..audio.wav import write_wav
from ..protocol.client import DEFAULT_STREAM_RETRIES, AbortFn, DeviceClient, RetryFn
from ..protocol.codec import StreamAborted, StreamHeader, StreamTrailer
from ..protocol.spec import STREAM_MAX_SECONDS
from ..transport.base import TransportTimeout
from .base import Session

INDEX_NAME = "index.csv"
INDEX_COLUMNS = ("chunk", "file", "stream", "seconds", "samples", "overrun", "err")

# Callback: (chunk_index_1_based, chunk_count, path) -> None, after a file is written.
ChunkFn = Callable[[int, int, Path], None]
# Callback: (bytes_received_overall, bytes_total_overall) -> None
ProgressFn = Callable[[int, int], None]
# Callback: (stream_index_1_based, header) -> None, once a stream is acknowledged.
AckFn = Callable[[int, StreamHeader], None]


@dataclass(frozen=True)
class ChunkResult:
    """One written file."""

    index: int  # 1-based
    path: Path
    stream: int  # 1-based index of the stream it was cut from
    byte_length: int
    sample_rate: int
    channels: int
    trailer: StreamTrailer | None = None

    @property
    def sample_count(self) -> int:
        return self.byte_length // 2

    @property
    def duration_s(self) -> float:
        return self.sample_count / max(self.channels, 1) / self.sample_rate


@dataclass
class BatchResult:
    """Outcome of a batch, complete or aborted."""

    directory: Path
    chunk_seconds: int
    requested: int
    chunks: list[ChunkResult] = field(default_factory=list)
    streams: int = 0

    @property
    def index_path(self) -> Path:
        return self.directory / INDEX_NAME

    @property
    def complete(self) -> bool:
        return len(self.chunks) == self.requested


def plan_streams(
    chunk_seconds: int, count: int, max_stream_s: int = STREAM_MAX_SECONDS
) -> list[int]:
    """How many chunks each successive ``stream`` should carry.

    Each stream is the longest that fits ``max_stream_s`` and holds a whole
    number of chunks; the last one takes the remainder.
    """
    if chunk_seconds <= 0:
        raise ValueError("chunk length must be positive")
    if chunk_seconds > max_stream_s:
        raise ValueError(f"chunk length must be <= {max_stream_s}s (the board's stream limit)")
    if count <= 0:
        raise ValueError("count must be positive")
    per_stream = max_stream_s // chunk_seconds
    plan: list[int] = []
    remaining = count
    while remaining > 0:
        k = min(per_stream, remaining)
        plan.append(k)
        remaining -= k
    return plan


def batch_directory(out_dir: str | Path, prefix: str = "batch") -> Path:
    """``<out_dir>/<prefix>-YYYYmmdd-HHMMSS``."""
    stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    return Path(out_dir) / f"{prefix}-{stamp}"


class BatchRecordSession(Session):
    """Drives one or more ``stream`` transfers and cuts them into equal WAV files."""

    def __init__(
        self,
        client: DeviceClient,
        out_dir: str | Path,
        *,
        max_stream_s: int = STREAM_MAX_SECONDS,
    ) -> None:
        self._client = client
        self._out_dir = Path(out_dir)
        self._max_stream_s = max_stream_s

    def run(self, chunk_seconds: int, count: int, **kwargs: object) -> BatchResult:
        return self.record(chunk_seconds, count, **kwargs)  # type: ignore[arg-type]

    def record(
        self,
        chunk_seconds: int,
        count: int,
        *,
        source: str = "mic",
        directory: Path | None = None,
        on_ack: AckFn | None = None,
        on_chunk: ChunkFn | None = None,
        on_progress: ProgressFn | None = None,
        should_abort: AbortFn | None = None,
        on_retry: RetryFn | None = None,
        retries: int = DEFAULT_STREAM_RETRIES,
    ) -> BatchResult:
        """Record ``count`` files of ``chunk_seconds`` each.

        Raises :class:`StreamAborted` (from the client) when ``should_abort``
        fires; files written before that point stay on disk and are listed in
        ``index.csv``. Any other transport error propagates the same way.
        """
        plan = plan_streams(chunk_seconds, count, self._max_stream_s)
        directory = directory if directory is not None else batch_directory(self._out_dir)
        directory.mkdir(parents=True, exist_ok=True)
        result = BatchResult(directory=directory, chunk_seconds=chunk_seconds, requested=count)
        self._write_index_header(result.index_path)

        # The header tells the real byte rate; until the first one arrives we
        # cannot size a chunk, so the total for the progress bar is derived from
        # it too and is the same for every stream of the batch.
        total_bytes = 0
        received = 0
        for stream_no, k in enumerate(plan, start=1):
            handle = self._client.start_stream(
                k * chunk_seconds,
                source=source,
                retries=retries,
                should_abort=should_abort,
                on_retry=on_retry,
            )
            header = handle.header
            result.streams = stream_no
            if on_ack is not None:
                on_ack(stream_no, header)
            bytes_per_second = header.sample_rate * header.channels * 2
            chunk_bytes = bytes_per_second * chunk_seconds
            if total_bytes == 0:
                total_bytes = bytes_per_second * chunk_seconds * count

            pending: list[ChunkResult] = []
            buf = bytearray()
            try:
                for piece in handle.chunks:
                    buf += piece
                    received += len(piece)
                    if on_progress is not None:
                        on_progress(received, total_bytes)
                    while len(buf) >= chunk_bytes and len(result.chunks) + len(pending) < count:
                        pcm = bytes(buf[:chunk_bytes])
                        del buf[:chunk_bytes]
                        self._write_chunk(result, pending, pcm, header, stream_no, on_chunk)
            except (StreamAborted, TransportTimeout):
                # The user pressed q, or the board stopped sending before the
                # header's byte count was met. A partial chunk in the buffer is
                # still audio: keep it as a short file when it is at least a
                # second long (the index shows its real length; anything shorter
                # is not a recording), and record every finished chunk of this
                # stream before letting the caller see the failure.
                if len(buf) >= bytes_per_second and len(result.chunks) + len(pending) < count:
                    self._write_chunk(result, pending, bytes(buf), header, stream_no, on_chunk)
                self._commit(result, pending, trailer=None)
                raise

            trailer = self._client.read_trailer()
            self._commit(result, pending, trailer)
        return result

    # -- helpers ------------------------------------------------------------

    def _commit(
        self, result: BatchResult, pending: list[ChunkResult], trailer: StreamTrailer | None
    ) -> None:
        """Move a stream's written chunks into the result and the index."""
        for chunk in pending:
            done = ChunkResult(
                index=chunk.index,
                path=chunk.path,
                stream=chunk.stream,
                byte_length=chunk.byte_length,
                sample_rate=chunk.sample_rate,
                channels=chunk.channels,
                trailer=trailer,
            )
            result.chunks.append(done)
            self._append_index(result.index_path, done)
        pending.clear()

    def _write_chunk(
        self,
        result: BatchResult,
        pending: list[ChunkResult],
        pcm: bytes,
        header: StreamHeader,
        stream_no: int,
        on_chunk: ChunkFn | None,
    ) -> None:
        """Write one file now and queue it for the index once the stream ends."""
        index = len(result.chunks) + len(pending) + 1
        path = result.directory / f"chunk-{index:03d}.wav"
        write_wav(path, pcm, sample_rate=header.sample_rate, channels=header.channels)
        pending.append(
            ChunkResult(
                index=index,
                path=path,
                stream=stream_no,
                byte_length=len(pcm),
                sample_rate=header.sample_rate,
                channels=header.channels,
            )
        )
        if on_chunk is not None:
            on_chunk(index, result.requested, path)

    @staticmethod
    def _write_index_header(path: Path) -> None:
        with path.open("w", newline="") as fh:
            csv.writer(fh).writerow(INDEX_COLUMNS)

    @staticmethod
    def _append_index(path: Path, chunk: ChunkResult) -> None:
        trailer = chunk.trailer
        with path.open("a", newline="") as fh:
            csv.writer(fh).writerow(
                (
                    chunk.index,
                    chunk.path.name,
                    chunk.stream,
                    f"{chunk.duration_s:.3f}",
                    chunk.sample_count,
                    "" if trailer is None else int(trailer.overrun),
                    "" if trailer is None else int(trailer.err),
                )
            )

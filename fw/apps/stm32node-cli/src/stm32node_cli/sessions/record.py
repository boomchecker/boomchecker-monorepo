"""Recording session: stream N seconds of PCM and save it as a WAV file."""

from __future__ import annotations

from collections.abc import Callable
from dataclasses import dataclass
from pathlib import Path

from ..audio.wav import timestamped_path, write_wav
from ..protocol.client import DeviceClient
from ..protocol.codec import StreamHeader
from .base import Session

# Callback: (bytes_received, bytes_total) -> None
ProgressFn = Callable[[int, int], None]


@dataclass(frozen=True)
class RecordResult:
    """Outcome of a completed recording."""

    path: Path
    header: StreamHeader
    byte_length: int

    @property
    def sample_count(self) -> int:
        return self.header.sample_count

    @property
    def duration_s(self) -> float:
        return self.header.duration_s


class RecordSession(Session):
    """Drives a ``stream <sec>`` transfer and writes the PCM to disk."""

    def __init__(self, client: DeviceClient, out_dir: str | Path) -> None:
        self._client = client
        self._out_dir = Path(out_dir)

    def run(
        self,
        seconds: int,
        *,
        source: str = "mic",
        on_progress: ProgressFn | None = None,
    ) -> RecordResult:
        return self.record(seconds, source=source, on_progress=on_progress)

    def record(
        self,
        seconds: int,
        *,
        source: str = "mic",
        on_progress: ProgressFn | None = None,
    ) -> RecordResult:
        handle = self._client.start_stream(seconds, source=source)
        total = handle.header.byte_length

        buf = bytearray()
        for chunk in handle.chunks:
            buf += chunk
            if on_progress is not None:
                on_progress(len(buf), total)

        path = timestamped_path(self._out_dir)
        write_wav(
            path,
            bytes(buf),
            sample_rate=handle.header.sample_rate,
            channels=handle.header.channels,
        )
        return RecordResult(path=path, header=handle.header, byte_length=len(buf))

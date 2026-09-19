"""Write raw PCM to standard RIFF/WAVE files."""

from __future__ import annotations

import wave
from datetime import datetime
from pathlib import Path

from ..protocol.spec import CHANNELS, SAMPLE_RATE_HZ, SAMPLE_WIDTH_BYTES


def write_wav(
    path: str | Path,
    pcm: bytes,
    *,
    sample_rate: int = SAMPLE_RATE_HZ,
    channels: int = CHANNELS,
    sample_width: int = SAMPLE_WIDTH_BYTES,
) -> Path:
    """Write ``pcm`` bytes as a WAV file and return the path."""
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), "wb") as wf:
        wf.setnchannels(channels)
        wf.setsampwidth(sample_width)
        wf.setframerate(sample_rate)
        wf.writeframes(pcm)
    return path


def timestamped_path(directory: str | Path, prefix: str = "rec", suffix: str = ".wav") -> Path:
    """Build ``<directory>/<prefix>-YYYYmmdd-HHMMSS<suffix>``."""
    stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    return Path(directory) / f"{prefix}-{stamp}{suffix}"

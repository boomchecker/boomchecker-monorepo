from __future__ import annotations

import os
import shutil
from pathlib import Path

try:
    from pydub import AudioSegment
    from pydub import utils as pydub_utils
except ImportError:  # pragma: no cover - dependency is optional
    AudioSegment = None
    pydub_utils = None


def _venv_bin_dir(base_dir: Path) -> Path:
    return base_dir / "venv" / "bin"


def prime_pydub_paths(base_dir: Path) -> None:
    """Configure pydub to prefer the local venv ffmpeg/ffprobe if present."""
    if AudioSegment is None or pydub_utils is None:
        return

    venv_bin = _venv_bin_dir(base_dir)
    venv_ffmpeg = venv_bin / "ffmpeg"
    venv_ffprobe = venv_bin / "ffprobe"
    if venv_ffmpeg.exists():
        AudioSegment.converter = str(venv_ffmpeg)
    if venv_ffprobe.exists():
        ffprobe_path = str(venv_ffprobe)
        pydub_utils.get_prober_name = lambda: ffprobe_path
        AudioSegment.ffprobe = ffprobe_path


def configure_audio_binaries(base_dir: Path) -> None:
    if AudioSegment is None:
        raise RuntimeError(
            "Missing dependency. Install `pydub` (plus ffmpeg) to decode audio."
        )

    # Check venv bin directory first, then environment variable, then PATH
    venv_bin = _venv_bin_dir(base_dir)
    venv_ffmpeg = venv_bin / "ffmpeg"
    venv_ffprobe = venv_bin / "ffprobe"

    ffmpeg_bin = (
        str(venv_ffmpeg)
        if venv_ffmpeg.exists()
        else os.environ.get("FFMPEG_BINARY") or shutil.which("ffmpeg")
    )
    ffprobe_bin = (
        str(venv_ffprobe)
        if venv_ffprobe.exists()
        else os.environ.get("FFPROBE_BINARY") or shutil.which("ffprobe")
    )
    if ffmpeg_bin:
        AudioSegment.converter = ffmpeg_bin
    if ffprobe_bin:
        AudioSegment.ffprobe = ffprobe_bin
        if pydub_utils is not None:
            pydub_utils.get_prober_name = lambda: ffprobe_bin

    missing = []
    if not ffmpeg_bin:
        missing.append("ffmpeg")
    if not ffprobe_bin:
        missing.append("ffprobe")
    if missing:
        missing_list = ", ".join(missing)
        raise RuntimeError(
            f"Missing {missing_list}. Install ffmpeg (includes ffprobe) "
            "or set FFMPEG_BINARY/FFPROBE_BINARY. "
            "Try `task setup` from scripts/median-filter."
        )

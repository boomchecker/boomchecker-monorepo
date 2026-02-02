"""Benchmark harness for median peak detector.

Runs the C detector over downloaded WAVs, stores peak timestamps, and exports
200 ms windows (20 ms pre-peak + 180 ms post-peak).
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
from array import array
from datetime import datetime
from pathlib import Path
from typing import Iterable

import ctypes

try:
    from pydub import AudioSegment
except ImportError:  # pragma: no cover - helpful message if dependency missing
    AudioSegment = None


ROOT_DIR = Path(__file__).resolve().parent.parent
PYTHON_DIR = Path(__file__).resolve().parent
DEFAULT_INPUT_DIR = PYTHON_DIR / "downloads"
DEFAULT_OUTPUT_DIR = PYTHON_DIR / "benchmark_out"
DEFAULT_LIB_PATH = ROOT_DIR / "build" / "libpeak.so"


class MedianDetectorLevels(ctypes.Structure):
    _fields_ = [
        ("det_level", ctypes.c_int16),
        ("det_rms", ctypes.c_int16),
        ("det_energy", ctypes.c_int16),
    ]


class MedianDetectorCfg(ctypes.Structure):
    _fields_ = [
        ("num_taps", ctypes.c_uint8),
        ("tap_size", ctypes.c_uint16),
        ("levels", MedianDetectorLevels),
    ]


def _configure_audio_binaries() -> None:
    if AudioSegment is None:
        raise RuntimeError(
            "Missing dependency. Install `pydub` (plus ffmpeg) to decode audio."
        )

    ffmpeg_bin = os.environ.get("FFMPEG_BINARY") or shutil.which("ffmpeg")
    ffprobe_bin = os.environ.get("FFPROBE_BINARY") or shutil.which("ffprobe")
    if ffmpeg_bin:
        AudioSegment.converter = ffmpeg_bin
    if ffprobe_bin:
        AudioSegment.ffprobe = ffprobe_bin

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


def _load_peak_lib(lib_path: Path) -> ctypes.CDLL:
    if not lib_path.exists():
        raise FileNotFoundError(
            f"Peak detector library not found: {lib_path}. "
            "Build it via `task csrc:build` from scripts/median-filter."
        )
    lib = ctypes.CDLL(str(lib_path))
    lib.detect_recording_i16.argtypes = [
        ctypes.POINTER(ctypes.c_int16),
        ctypes.c_size_t,
        ctypes.POINTER(MedianDetectorCfg),
        ctypes.POINTER(ctypes.c_int),
        ctypes.c_size_t,
    ]
    lib.detect_recording_i16.restype = ctypes.c_int
    return lib


def _iter_wavs(input_dir: Path) -> Iterable[Path]:
    if not input_dir.exists():
        return []
    return sorted(p for p in input_dir.iterdir() if p.suffix.lower() == ".wav")


def _load_metadata(wav_path: Path) -> dict:
    meta_path = wav_path.with_suffix(".json")
    if not meta_path.exists():
        return {}
    try:
        return json.loads(meta_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return {}


def _to_mono_int16(audio: AudioSegment, target_sr: int | None) -> AudioSegment:
    if audio.channels != 1:
        audio = audio.set_channels(1)
    if audio.sample_width != 2:
        audio = audio.set_sample_width(2)
    if target_sr is not None and audio.frame_rate != target_sr:
        audio = audio.set_frame_rate(target_sr)
    return audio


def _make_window(samples: array, start: int, length: int) -> array:
    total = len(samples)
    end = start + length
    pad_left = max(0, -start)
    pad_right = max(0, end - total)
    start = max(0, start)
    end = min(total, end)

    window = array("h")
    if pad_left:
        window.extend([0] * pad_left)
    if start < end:
        window.extend(samples[start:end])
    if pad_right:
        window.extend([0] * pad_right)
    return window


def _write_jsonl(path: Path, record: dict) -> None:
    with path.open("a", encoding="utf-8") as handle:
        handle.write(json.dumps(record, ensure_ascii=False) + "\n")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Benchmark median peak detector and export 200 ms windows."
    )
    parser.add_argument("--input-dir", type=Path, default=DEFAULT_INPUT_DIR)
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT_DIR)
    parser.add_argument("--lib-path", type=Path, default=DEFAULT_LIB_PATH)
    parser.add_argument("--window-ms", type=int, default=200)
    parser.add_argument("--pre-ms", type=int, default=20)
    parser.add_argument("--sample-rate", type=int, default=None)
    parser.add_argument("--num-taps", type=int, default=31)
    parser.add_argument("--tap-size", type=int, default=30)
    parser.add_argument("--det-level", type=int, default=2000)
    parser.add_argument("--det-rms", type=int, default=3)
    parser.add_argument("--det-energy", type=int, default=2)
    parser.add_argument("--max-videos", type=int, default=None)
    args = parser.parse_args()

    _configure_audio_binaries()
    lib = _load_peak_lib(args.lib_path)

    input_dir = args.input_dir
    output_dir = args.output_dir
    output_dir.mkdir(parents=True, exist_ok=True)
    windows_dir = output_dir / "windows"
    windows_dir.mkdir(parents=True, exist_ok=True)

    results_path = output_dir / "results.jsonl"
    summary_path = output_dir / "videos.jsonl"
    if results_path.exists():
        results_path.unlink()
    if summary_path.exists():
        summary_path.unlink()

    cfg = MedianDetectorCfg(
        num_taps=args.num_taps,
        tap_size=args.tap_size,
        levels=MedianDetectorLevels(
            det_level=args.det_level,
            det_rms=args.det_rms,
            det_energy=args.det_energy,
        ),
    )

    run_config = {
        "ts": datetime.utcnow().isoformat(timespec="seconds") + "Z",
        "input_dir": str(input_dir),
        "output_dir": str(output_dir),
        "lib_path": str(args.lib_path),
        "window_ms": args.window_ms,
        "pre_ms": args.pre_ms,
        "sample_rate": args.sample_rate,
        "num_taps": args.num_taps,
        "tap_size": args.tap_size,
        "det_level": args.det_level,
        "det_rms": args.det_rms,
        "det_energy": args.det_energy,
    }
    (output_dir / "run_config.json").write_text(
        json.dumps(run_config, ensure_ascii=False, indent=2), encoding="utf-8"
    )

    wavs = list(_iter_wavs(input_dir))
    if args.max_videos is not None:
        wavs = wavs[: args.max_videos]

    if not wavs:
        print(f"No WAV files found in {input_dir}")
        return

    for wav_path in wavs:
        metadata = _load_metadata(wav_path)
        video_id = metadata.get("id") or wav_path.stem
        title = metadata.get("title")
        url = metadata.get("webpage_url")

        audio = AudioSegment.from_file(wav_path)
        audio = _to_mono_int16(audio, args.sample_rate)
        samples = audio.get_array_of_samples()
        total_samples = len(samples)
        if total_samples < args.tap_size:
            _write_jsonl(
                summary_path,
                {
                    "video_id": video_id,
                    "title": title,
                    "url": url,
                    "source_path": str(wav_path),
                    "sample_rate": audio.frame_rate,
                    "duration_s": audio.duration_seconds,
                    "hits": 0,
                    "error": "recording shorter than tap_size",
                },
            )
            continue

        max_positions = max(1, total_samples // args.tap_size)
        positions = (ctypes.c_int * max_positions)()
        samples_ptr = (ctypes.c_int16 * total_samples).from_buffer(samples)
        hits = lib.detect_recording_i16(
            samples_ptr,
            total_samples,
            ctypes.byref(cfg),
            positions,
            max_positions,
        )
        if hits < 0:
            _write_jsonl(
                summary_path,
                {
                    "video_id": video_id,
                    "title": title,
                    "url": url,
                    "source_path": str(wav_path),
                    "sample_rate": audio.frame_rate,
                    "duration_s": audio.duration_seconds,
                    "hits": 0,
                    "error": f"detector error {hits}",
                },
            )
            continue

        hit_count = min(hits, max_positions)
        pre_samples = int(round(args.pre_ms * audio.frame_rate / 1000))
        window_samples = int(round(args.window_ms * audio.frame_rate / 1000))

        for idx in range(hit_count):
            peak_sample = int(positions[idx])
            peak_time_s = peak_sample / audio.frame_rate
            window_start = peak_sample - pre_samples
            window = _make_window(samples, window_start, window_samples)

            window_seg = AudioSegment(
                data=window.tobytes(),
                sample_width=2,
                frame_rate=audio.frame_rate,
                channels=1,
            )
            window_name = f"{video_id}_peak_{idx:03d}_{peak_sample}.wav"
            window_path = windows_dir / window_name
            window_seg.export(window_path, format="wav")

            _write_jsonl(
                results_path,
                {
                    "video_id": video_id,
                    "title": title,
                    "url": url,
                    "source_path": str(wav_path),
                    "peak_index": idx,
                    "peak_sample": peak_sample,
                    "peak_time_s": peak_time_s,
                    "sample_rate": audio.frame_rate,
                    "window_ms": args.window_ms,
                    "pre_ms": args.pre_ms,
                    "window_path": str(window_path),
                },
            )

        _write_jsonl(
            summary_path,
            {
                "video_id": video_id,
                "title": title,
                "url": url,
                "source_path": str(wav_path),
                "sample_rate": audio.frame_rate,
                "duration_s": audio.duration_seconds,
                "hits": hit_count,
                "hits_truncated": hits > max_positions,
            },
        )

        print(f"{video_id}: hits={hit_count} duration={audio.duration_seconds:.2f}s")


if __name__ == "__main__":
    main()

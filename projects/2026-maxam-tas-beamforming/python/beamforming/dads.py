"""DADS drone clips: download a fixed selection once, then load from ``data/dads``.

Dataset: ``geronimobasso/drone-audio-detection-samples`` (Hugging Face), 39 parquet shards of
mono 16 kHz PCM16 WAV bytes, mostly 0.5 s long. Shards are label-pure, so the loader does
not read labels: ``--shard`` must be a drone shard (label 1, verified for 20 and 38; shard 0
is label 0). Clips are drawn from evenly spaced row groups (about 100 rows, 1.6 MB each) so
that one shard gives a varied selection without downloading it whole (1.3 GB).

The audio is decoded with ``soundfile`` from the raw bytes, which avoids the torchcodec
dependency that ``datasets`` 5.x needs for decoded ``Audio`` columns.
"""

from __future__ import annotations

import io
import json
import os
from dataclasses import dataclass
from pathlib import Path

import numpy as np
import soundfile as sf
from numpy.typing import NDArray

from . import signals as sg

REPO = "geronimobasso/drone-audio-detection-samples"
SHARD_COUNT = 39
DEFAULT_SHARD = 20
DEFAULT_OUT = Path("data/dads")
MANIFEST = "manifest.json"
MIN_SAMPLES = sg.SEGMENT + 2 * sg.MARGIN  # shortest clip observe() can use
MIN_BAND_FRACTION = 0.2  # share of clip energy inside the 300 to 2000 Hz localisation band


@dataclass(frozen=True)
class Clip:
    """One cached drone clip."""

    index: int
    path: Path
    n_samples: int
    band_fraction: float


def band_fraction(x: NDArray, fs: int = sg.FS, band: tuple[float, float] = sg.BAND_HZ) -> float:
    """Share of the clip's energy inside ``band`` (0 for an all zero clip)."""
    total = float(np.sum(np.asarray(x, dtype=float) ** 2))
    if total == 0.0:
        return 0.0
    n = len(x)
    spec = np.abs(np.fft.rfft(x)) ** 2
    freqs = np.fft.rfftfreq(n, 1 / fs)
    return float(np.sum(spec[(freqs >= band[0]) & (freqs <= band[1])]) / np.sum(spec))


def _decode(raw: bytes) -> tuple[NDArray[np.float32], int]:
    data, fs = sf.read(io.BytesIO(raw), dtype="float32", always_2d=False)
    if data.ndim > 1:
        data = data.mean(axis=1, dtype=np.float32)
    return data, int(fs)


def fetch(
    n: int = 100,
    out_dir: Path = DEFAULT_OUT,
    shard: int = DEFAULT_SHARD,
    seed: int = 0,
    row_groups: int = 20,
) -> dict:
    """Download ``n`` drone clips into ``out_dir`` and write ``manifest.json``.

    Candidates come from ``row_groups`` evenly spaced row groups of one shard, a fixed number
    of random rows (seeded) from each; clips shorter than :data:`MIN_SAMPLES`, with another
    sample rate or with too little energy in the localisation band are skipped.
    """
    import pyarrow.parquet as pq
    from huggingface_hub import HfFileSystem

    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    fs = HfFileSystem(token=os.environ.get("HF_TOKEN") or None)
    path = f"datasets/{REPO}/data/train-{shard:05d}-of-{SHARD_COUNT:05d}.parquet"
    rng = np.random.default_rng(seed)
    per_group = int(np.ceil(1.5 * n / row_groups))  # oversample, some rows are filtered out

    clips: list[dict] = []
    with fs.open(path, block_size=4 * 2**20) as handle:
        pf = pq.ParquetFile(handle)
        groups = np.unique(np.linspace(0, pf.num_row_groups - 1, row_groups).round().astype(int))
        for rg in groups:
            if len(clips) >= n:
                break
            rows = pf.read_row_group(int(rg), columns=["audio"]).column("audio").to_pylist()
            for row in rng.permutation(len(rows))[:per_group]:
                if len(clips) >= n:
                    break
                raw = rows[int(row)]["bytes"]
                x, rate = _decode(raw)
                frac = band_fraction(x)
                if rate != sg.FS or len(x) < MIN_SAMPLES or frac < MIN_BAND_FRACTION:
                    continue
                name = f"{len(clips):03d}.wav"
                (out_dir / name).write_bytes(raw)
                clips.append(
                    {
                        "file": name,
                        "source": rows[int(row)]["path"],
                        "row_group": int(rg),
                        "row": int(row),
                        "n_samples": len(x),
                        "rms": float(np.sqrt(np.mean(x**2))),
                        "band_fraction": frac,
                    }
                )
            print(f"row group {int(rg):2d}: {len(clips)}/{n} clips", flush=True)
    if len(clips) < n:
        raise RuntimeError(f"only {len(clips)} usable clips in shard {shard}, wanted {n}")
    manifest = {
        "dataset": REPO,
        "shard": shard,
        "seed": seed,
        "n": n,
        "min_samples": MIN_SAMPLES,
        "min_band_fraction": MIN_BAND_FRACTION,
        "clips": clips,
    }
    (out_dir / MANIFEST).write_text(json.dumps(manifest, indent=2) + "\n")
    return manifest


def list_clips(data_dir: Path = DEFAULT_OUT) -> list[Clip]:
    """Cached clips in manifest order; raises with the fix when nothing is downloaded."""
    data_dir = Path(data_dir)
    manifest_path = data_dir / MANIFEST
    if not manifest_path.exists():
        raise FileNotFoundError(f"{manifest_path} missing: run `task sim:fetch` first")
    manifest = json.loads(manifest_path.read_text())
    return [
        Clip(i, data_dir / c["file"], c["n_samples"], c["band_fraction"])
        for i, c in enumerate(manifest["clips"])
    ]


def load_clip(clip: Clip | Path) -> NDArray[np.float64]:
    """Clip samples as float64 in [-1, 1) at 16 kHz."""
    path = clip.path if isinstance(clip, Clip) else Path(clip)
    x, rate = _decode(path.read_bytes())
    if rate != sg.FS:
        raise ValueError(f"{path} has sample rate {rate}, expected {sg.FS}")
    return x.astype(np.float64)


def random_offset(n_samples: int, rng: np.random.Generator, n: int = sg.SEGMENT) -> int:
    """Random start of a 100 ms segment that keeps :data:`signals.MARGIN` samples on both sides."""
    lo, hi = sg.MARGIN, n_samples - n - sg.MARGIN
    if hi < lo:
        raise ValueError(f"clip of {n_samples} samples is too short for a {n} sample segment")
    return int(rng.integers(lo, hi + 1))

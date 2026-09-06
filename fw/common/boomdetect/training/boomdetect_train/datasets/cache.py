"""The frame cache: the front end's output for every clip, computed once.

Per clip the cache holds one float32 row per frame, FRAME_WIDTH wide:

    [rms | mfcc x 13 | logmel x 20 | spectral scalars x 8]

which is everything any layout needs (see features.py): layout 1 from the MFCC
block, layout 2 from MFCC, scalars and log-mel, layout 3 from log-mel. The raw
magnitude spectra are not kept - at 513 float64 per frame they would be most of
a gigabyte, and the scalars are the only thing derived from them.

Storage is one .npz per source: a (total_frames, FRAME_WIDTH) matrix plus an
index table (clip id -> offset, count). Tens of thousands of tiny files were
the alternative and are slow on every filesystem.
"""

from __future__ import annotations

import io
from dataclasses import dataclass
from pathlib import Path

import numpy as np
import pandas as pd
import pyarrow.parquet as pq
from tqdm import tqdm

from boomdetect_train.dsp.audio import read_audio, to_16k
from boomdetect_train.dsp.mfcc import N_MELS, N_MFCC, FrameData, Frontend
from boomdetect_train.features import N_SCALARS, frame_scalars_batch
from boomdetect_train.paths import cache_dir

COL_RMS = 0
COL_MFCC = slice(1, 1 + N_MFCC)
COL_LOGMEL = slice(1 + N_MFCC, 1 + N_MFCC + N_MELS)
COL_SCALARS = slice(1 + N_MFCC + N_MELS, 1 + N_MFCC + N_MELS + N_SCALARS)
FRAME_WIDTH = 1 + N_MFCC + N_MELS + N_SCALARS  # 42


def frame_rows(frames: FrameData) -> np.ndarray:
    """Pack a FrameData into cache rows."""
    scal = frame_scalars_batch(frames.mag)
    return np.concatenate([frames.rms[:, None], frames.mfcc, frames.logmel, scal], axis=1).astype(
        np.float32
    )


@dataclass
class CachedFrames:
    """Cache rows of one clip, viewed as the pieces the feature layouts need."""

    rows: np.ndarray  # (F, FRAME_WIDTH)

    @property
    def n(self) -> int:
        return int(self.rows.shape[0])

    @property
    def rms(self) -> np.ndarray:
        return self.rows[:, COL_RMS]

    @property
    def mfcc(self) -> np.ndarray:
        return self.rows[:, COL_MFCC]

    @property
    def logmel(self) -> np.ndarray:
        return self.rows[:, COL_LOGMEL]

    @property
    def scalars(self) -> np.ndarray:
        return self.rows[:, COL_SCALARS]


def _load_clip(path: str) -> tuple[np.ndarray, int]:
    """Audio of a manifest `path`: a file, or `<parquet>#<row>`."""
    if "#" in path and path.rsplit("#", 1)[1].isdigit():
        shard, row = path.rsplit("#", 1)
        table = pq.read_table(shard, columns=["audio"])
        audio = table.column("audio")[int(row)].as_py()
        return read_audio(audio["bytes"])
    return read_audio(path)


def _load_shard_clips(shard: str, rows: list[int]) -> dict[int, bytes]:
    """All requested rows of one shard in one read; row-by-row was quadratic."""
    table = pq.read_table(shard, columns=["audio"])
    col = table.column("audio")
    return {r: col[r].as_py()["bytes"] for r in rows}


class FrameCache:
    """Read side: the per-source .npz files and their index."""

    def __init__(self, root: Path | None = None):
        self.root = root if root is not None else cache_dir() / "frames"
        self._data: dict[str, np.ndarray] = {}
        self._index: dict[str, pd.DataFrame] = {}

    def path_for(self, source: str) -> Path:
        return self.root / f"{source}.npz"

    def has(self, source: str) -> bool:
        return self.path_for(source).exists()

    def _ensure(self, source: str) -> None:
        if source in self._data:
            return
        with np.load(self.path_for(source), allow_pickle=False) as z:
            self._data[source] = z["frames"]
            idx = pd.DataFrame(
                {"id": z["ids"].astype(str), "offset": z["offsets"], "count": z["counts"]}
            )
        self._index[source] = idx.set_index("id")

    def get(self, source: str, clip_id: str) -> CachedFrames:
        self._ensure(source)
        rec = self._index[source].loc[clip_id]
        off, cnt = int(rec["offset"]), int(rec["count"])
        return CachedFrames(self._data[source][off : off + cnt])

    def ids(self, source: str) -> list[str]:
        self._ensure(source)
        return list(self._index[source].index)


def build_source_cache(
    manifest: pd.DataFrame,
    source: str,
    frontend: Frontend,
    root: Path | None = None,
    *,
    show_progress: bool = True,
) -> Path:
    """Run the front end over every clip of `source` and write its .npz."""
    cache = FrameCache(root)
    cache.root.mkdir(parents=True, exist_ok=True)
    sub = manifest[manifest["source"] == source]
    ids: list[str] = []
    offsets: list[int] = []
    counts: list[int] = []
    blocks: list[np.ndarray] = []
    total = 0

    # Parquet-backed clips are read shard by shard; file clips one by one.
    shard_rows: dict[str, list[tuple[str, int]]] = {}
    file_items: list[tuple[str, str]] = []
    for rec in sub.itertuples(index=False):
        if "#" in rec.path and rec.path.rsplit("#", 1)[1].isdigit():
            shard, row = rec.path.rsplit("#", 1)
            shard_rows.setdefault(shard, []).append((rec.id, int(row)))
        else:
            file_items.append((rec.id, rec.path))

    def add(cid: str, x: np.ndarray, sr: int) -> None:
        nonlocal total
        x16 = to_16k(x, sr)
        rows = frame_rows(frontend.process(x16))
        ids.append(cid)
        offsets.append(total)
        counts.append(rows.shape[0])
        blocks.append(rows)
        total += rows.shape[0]

    for shard, items in shard_rows.items():
        table = pq.read_table(shard, columns=["audio"])
        col = table.column("audio")
        it = tqdm(items, desc=f"{source} {Path(shard).stem}", disable=not show_progress)
        for cid, row in it:
            x, sr = read_audio(col[row].as_py()["bytes"])
            add(cid, x, sr)
    it = tqdm(file_items, desc=source, disable=not show_progress)
    for cid, path in it:
        x, sr = read_audio(path)
        add(cid, x, sr)

    frames = np.concatenate(blocks, axis=0) if blocks else np.empty((0, FRAME_WIDTH), np.float32)
    out = cache.path_for(source)
    np.savez(
        out,
        frames=frames.astype(np.float32),
        ids=np.asarray(ids, dtype=str),
        offsets=np.asarray(offsets, dtype=np.int64),
        counts=np.asarray(counts, dtype=np.int64),
    )
    return out


def load_clip_audio(path: str) -> tuple[np.ndarray, int]:
    """Public wrapper for one-off reads (evaluation of a single file, plots)."""
    return _load_clip(path)


def bytes_to_audio(b: bytes) -> tuple[np.ndarray, int]:
    return read_audio(io.BytesIO(b).getvalue())

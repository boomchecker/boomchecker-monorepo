"""The frame cache: the front end's output for every clip, computed once.

Per clip the cache holds one float32 row per frame, FRAME_WIDTH wide and cut
into the COL_* slices below: everything the layouts read except the band
spectrograms, which have their own cache (build_spec_cache). The raw magnitude
spectra are not kept - at 513 float64 per frame they would be most of a
gigabyte, and the scalars are the only thing derived from them.

Storage is one .npz per source: a (total_frames, FRAME_WIDTH) matrix plus an
index table (clip id -> offset, count). Tens of thousands of tiny files were
the alternative and are slow on every filesystem.
"""

from __future__ import annotations

from collections.abc import Callable, Iterator
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np
import pandas as pd
import pyarrow.parquet as pq
from tqdm import tqdm

from boomdetect_train.datasets.field import is_field_path, load_field_audio
from boomdetect_train.dsp.audio import read_audio, to_16k
from boomdetect_train.dsp.mfcc import ENV_PER_FRAME, N_MELS, N_MFCC, FrameData, Frontend
from boomdetect_train.dsp.spectro import SPEC_FE_NAMES, SPEC_N_BANDS, spec_frames
from boomdetect_train.features import N_SCALARS, frame_scalars_batch
from boomdetect_train.paths import cache_dir

COL_RMS = 0
COL_MFCC = slice(1, 1 + N_MFCC)
COL_LOGMEL = slice(1 + N_MFCC, 1 + N_MFCC + N_MELS)
COL_SCALARS = slice(1 + N_MFCC + N_MELS, 1 + N_MFCC + N_MELS + N_SCALARS)
# The 1-4 kHz envelope at 1 kHz, 32 samples per hop: what the modulation features
# of layout 4 read over the last two seconds of frames (features.modulation_stats).
COL_ENV = slice(1 + N_MFCC + N_MELS + N_SCALARS, 1 + N_MFCC + N_MELS + N_SCALARS + ENV_PER_FRAME)
FRAME_WIDTH = 1 + N_MFCC + N_MELS + N_SCALARS + ENV_PER_FRAME  # 74


def frame_rows(frames: FrameData) -> np.ndarray:
    """Pack a FrameData into cache rows."""
    scal = frame_scalars_batch(frames.mag)
    return np.concatenate(
        [frames.rms[:, None], frames.mfcc, frames.logmel, scal, frames.env], axis=1
    ).astype(np.float32)


@dataclass
class CachedFrames:
    """Cache rows of one clip, viewed as the pieces the feature layouts need."""

    rows: np.ndarray  # (F, FRAME_WIDTH)
    # Band spectrograms (dsp/spectro.py) by front-end name, (F, SPEC_N_BANDS):
    # given directly for frames computed on the fly, or fetched on demand from
    # the spectrogram cache for cached clips.
    specs: dict[str, np.ndarray] | None = None
    spec_loader: Callable[[str], np.ndarray] | None = field(default=None, repr=False)

    def spec(self, name: str) -> np.ndarray:
        """(F, SPEC_N_BANDS) float32 log band power of front-end `name`."""
        if self.specs is not None and name in self.specs:
            return self.specs[name]
        if self.spec_loader is None:
            raise KeyError(f"no {name} spectrogram for these frames")
        s = np.asarray(self.spec_loader(name), dtype=np.float32)
        if self.specs is None:
            self.specs = {}
        self.specs[name] = s
        return s

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

    @property
    def env(self) -> np.ndarray:
        return self.rows[:, COL_ENV]


def load_clip_audio(path: str) -> tuple[np.ndarray, int]:
    """Audio of a manifest `path`: a file, `<parquet>#<row>`, or a field clip spec."""
    if "#" in path and path.rsplit("#", 1)[1].isdigit():
        shard, row = path.rsplit("#", 1)
        table = pq.read_table(shard, columns=["audio"])
        audio = table.column("audio")[int(row)].as_py()
        return read_audio(audio["bytes"])
    if is_field_path(path):
        return load_field_audio(path)
    return read_audio(path)


class FrameCache:
    """Read side: the per-source .npz files and their index."""

    def __init__(self, root: Path | None = None):
        self.root = root if root is not None else cache_dir() / "frames"
        self.spec_root = self.root.parent / "spec"
        self._data: dict[str, np.ndarray] = {}
        self._index: dict[str, pd.DataFrame] = {}
        self._spec: dict[tuple[str, str], np.ndarray] = {}

    def path_for(self, source: str) -> Path:
        return self.root / f"{source}.npz"

    def has(self, source: str) -> bool:
        return self.path_for(source).exists()

    def _ensure(self, source: str) -> None:
        if source in self._data:
            return
        with np.load(self.path_for(source), allow_pickle=False) as z:
            self._data[source] = z["frames"]
            if self._data[source].shape[1] != FRAME_WIDTH:
                raise ValueError(
                    f"{self.path_for(source)} has {self._data[source].shape[1]} columns per frame, "
                    f"this code expects {FRAME_WIDTH}: run `bdtrain features {source} --force`"
                )
            idx = pd.DataFrame(
                {"id": z["ids"].astype(str), "offset": z["offsets"], "count": z["counts"]}
            )
        self._index[source] = idx.set_index("id")

    def spec_path(self, name: str, source: str) -> Path:
        return self.spec_root / name / f"{source}.npz"

    def _spec_rows(self, name: str, source: str) -> np.ndarray:
        key = (name, source)
        if key not in self._spec:
            p = self.spec_path(name, source)
            if not p.exists():
                raise FileNotFoundError(f"{p}: run `bdtrain features {source} --spec`")
            self._ensure(source)
            with np.load(p, allow_pickle=False) as z:
                if not np.array_equal(z["counts"], self._index[source]["count"].to_numpy()):
                    raise ValueError(f"{p} does not match the frame cache: rebuild with --spec")
                self._spec[key] = z["spec"]
        return self._spec[key]

    def drop_loaded(self) -> None:
        """Forget everything loaded (frames, index, spectrograms); it reloads on the next get.

        A run holds its windows once they are built; the HuggingFace frames alone
        are ~2 GB that two parallel runs need not keep.
        """
        self._spec.clear()
        self._data.clear()
        self._index.clear()

    def get(self, source: str, clip_id: str) -> CachedFrames:
        self._ensure(source)
        rec = self._index[source].loc[clip_id]
        off, cnt = int(rec["offset"]), int(rec["count"])

        def loader(name: str) -> np.ndarray:
            return self._spec_rows(name, source)[off : off + cnt]

        return CachedFrames(self._data[source][off : off + cnt], spec_loader=loader)


def _source_parts(
    manifest: pd.DataFrame, source: str
) -> tuple[dict[str, list[tuple[str, int]]], list[tuple[str, str]]]:
    """Clips of `source` as parquet shard -> (id, row) and (id, path) of file clips.

    The frame cache's order is every shard's clips in turn, then the files.
    """
    sub = manifest[manifest["source"] == source]
    shard_rows: dict[str, list[tuple[str, int]]] = {}
    file_items: list[tuple[str, str]] = []
    for rec in sub.itertuples(index=False):
        if "#" in rec.path and rec.path.rsplit("#", 1)[1].isdigit():
            shard, row = rec.path.rsplit("#", 1)
            shard_rows.setdefault(shard, []).append((rec.id, int(row)))
        else:
            file_items.append((rec.id, rec.path))
    return shard_rows, file_items


def iter_source_audio(manifest: pd.DataFrame, source: str) -> Iterator[tuple[str, np.ndarray]]:
    """(clip id, 16 kHz audio) of every clip of `source`, in the frame cache's order."""
    shard_rows, file_items = _source_parts(manifest, source)
    for shard, items in shard_rows.items():
        table = pq.read_table(shard, columns=["audio"])
        col = table.column("audio")
        it = tqdm(items, desc=f"{source} {Path(shard).stem}")
        for cid, row in it:
            x, sr = read_audio(col[row].as_py()["bytes"])
            yield cid, to_16k(x, sr)
    it = tqdm(file_items, desc=source)
    for cid, path in it:
        x, sr = load_clip_audio(path)
        yield cid, to_16k(x, sr)


def _index_arrays(ids: list[str], counts: list[int]) -> dict[str, np.ndarray]:
    c = np.asarray(counts, dtype=np.int64)
    offsets = np.concatenate([[0], np.cumsum(c)[:-1]]) if c.size else np.empty(0)
    return {
        "ids": np.asarray(ids, dtype=str),
        "offsets": offsets.astype(np.int64),
        "counts": c,
    }


def build_source_cache(manifest: pd.DataFrame, source: str, frontend: Frontend) -> Path:
    """Run the front end over every clip of `source` and write its .npz."""
    cache = FrameCache()
    cache.root.mkdir(parents=True, exist_ok=True)
    ids: list[str] = []
    counts: list[int] = []
    blocks: list[np.ndarray] = []
    for cid, x16 in iter_source_audio(manifest, source):
        rows = frame_rows(frontend.process(x16))
        ids.append(cid)
        counts.append(rows.shape[0])
        blocks.append(rows)

    frames = np.concatenate(blocks, axis=0) if blocks else np.empty((0, FRAME_WIDTH), np.float32)
    out = cache.path_for(source)
    np.savez(out, frames=frames.astype(np.float32), **_index_arrays(ids, counts))
    return out


def _spec_task(task: tuple, names: tuple[str, ...]) -> tuple[list[str], list[int], dict]:
    """One worker's share of build_spec_cache: a parquet shard or a run of file clips."""
    kind, where, items = task
    ids: list[str] = []
    counts: list[int] = []
    blocks: dict[str, list[np.ndarray]] = {n: [] for n in names}

    def add(cid: str, x16: np.ndarray) -> None:
        specs = spec_frames(x16, names)
        ids.append(cid)
        counts.append(next(iter(specs.values())).shape[0])
        for n in names:
            blocks[n].append(specs[n].astype(np.float16))

    if kind == "shard":
        col = pq.read_table(where, columns=["audio"]).column("audio")
        for cid, row in items:
            x, sr = read_audio(col[row].as_py()["bytes"])
            add(cid, to_16k(x, sr))
    else:
        for cid, path in items:
            x, sr = load_clip_audio(path)
            add(cid, to_16k(x, sr))
    empty = np.empty((0, SPEC_N_BANDS), np.float16)
    return ids, counts, {n: np.concatenate(b) if b else empty for n, b in blocks.items()}


def build_spec_cache(
    manifest: pd.DataFrame,
    source: str,
    names: tuple[str, ...] | list[str] = SPEC_FE_NAMES,
    *,
    workers: int = 1,
) -> list[Path]:
    """Every band spectrogram of `source` in one pass over its audio, float16 per front-end.

    Frame for frame the frame cache's rows (same ids, offsets and counts), which
    is checked when they are read back. `workers` > 1 spreads the shards (and
    runs of file clips) over processes; the order of the result does not change.
    """
    from concurrent.futures import ProcessPoolExecutor

    names = tuple(names)
    cache = FrameCache()
    shard_rows, file_items = _source_parts(manifest, source)
    tasks: list[tuple] = [("shard", shard, items) for shard, items in shard_rows.items()]
    step = 200
    tasks += [("files", "", file_items[i : i + step]) for i in range(0, len(file_items), step)]
    ids: list[str] = []
    counts: list[int] = []
    blocks: dict[str, list[np.ndarray]] = {n: [] for n in names}
    if workers > 1 and len(tasks) > 1:
        pool = ProcessPoolExecutor(max_workers=workers)
        results = pool.map(_spec_task, tasks, [names] * len(tasks))
    else:
        pool = None
        results = (_spec_task(t, names) for t in tasks)
    for t_ids, t_counts, t_blocks in tqdm(results, total=len(tasks), desc=f"{source} spec"):
        ids += t_ids
        counts += t_counts
        for n in names:
            blocks[n].append(t_blocks[n])
    if pool is not None:
        pool.shutdown()
    index = _index_arrays(ids, counts)
    written = []
    for n in names:
        out = cache.spec_path(n, source)
        out.parent.mkdir(parents=True, exist_ok=True)
        empty = np.empty((0, SPEC_N_BANDS), np.float16)
        spec = np.concatenate(blocks[n], axis=0) if blocks[n] else empty
        np.savez(out, spec=spec, **index)
        written.append(out)
        blocks[n] = []
    return written

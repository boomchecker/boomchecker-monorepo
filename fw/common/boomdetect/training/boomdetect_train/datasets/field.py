"""The node's own field recordings: labelled sessions recorded with `stm32node-cli record`.

One directory per session under raw/field/, named by its date:

    raw/field/<session>/Positive/<name>/chunk-NNN.wav + index.csv   a `record <s> <n>` batch
    raw/field/<session>/Positive/<name>.wav                        one continuous recording
    raw/field/<session>/Negative/...                               the same, for negatives

Everything else in a session directory is ignored, so notes and scoring dumps
can live next to the audio.

A folder is one continuous recording (one stream of the board), and that makes
it one leakage group: its chunks are consecutive seconds of the same audio, the
trap the HuggingFace set taught (sources.hf_group). Chunks removed by hand -
the first chunk of every stream carries the PDM start transient - split a
folder into runs of consecutive chunks; each run becomes one clip, and every
run of a folder keeps the folder's group, so a recording never straddles a fold.

The drone is read from the name's prefix (dji_blizko -> dji_phantom4), because
that is how the recordings are named in the field; the category of a negative
is its folder name, so a report can say which sound fools a model.
"""

from __future__ import annotations

import csv
import hashlib
from collections.abc import Iterator
from pathlib import Path

import numpy as np
import soundfile as sf

from boomdetect_train.dsp.audio import read_audio

SOURCE = "field"
SPLIT = "field"
LABEL_DIRS = {"positive": 1, "negative": 0}

# Name prefix -> drone model. A positive whose name matches none of them is
# still a drone, just not one this table knows.
DRONE_PREFIXES = {"dji": "dji_phantom4", "runner": "runner250"}
UNKNOWN_DRONE = "drone"

# The PDM chain needs ~0.12 s to settle after a stream starts (clipped
# transient, DC step); a clip that begins at the first sample of a stream
# drops this much.
START_SKIP_S = 0.2

CHUNK_GLOB = "chunk-*.wav"


def drone_of(name: str) -> str:
    """dji_blizko -> dji_phantom4, runner_zboku -> runner250, anything else -> drone."""
    prefix = name.lower().split("_", 1)[0].split("-", 1)[0]
    return DRONE_PREFIXES.get(prefix, UNKNOWN_DRONE)


def chunk_number(p: Path) -> int:
    return int(p.stem.split("-", 1)[1])


def chunk_runs(folder: Path) -> list[tuple[int, int]]:
    """(first, last) chunk numbers of every run of consecutive chunks still on disk.

    With an index.csv a run also ends where the stream changes (a batch longer
    than one stream is several streams back to back, with a gap between them).
    """
    present = {chunk_number(p) for p in folder.glob(CHUNK_GLOB)}
    stream_of: dict[int, str] = {}
    index = folder / "index.csv"
    if index.exists():
        with open(index, newline="", encoding="utf-8") as f:
            for row in csv.DictReader(f):
                stream_of[int(row["chunk"])] = str(row.get("stream", ""))
    runs: list[tuple[int, int]] = []
    start = prev = None
    for n in sorted(present):
        same_stream = prev is not None and stream_of.get(n) == stream_of.get(prev)
        if prev is not None and n == prev + 1 and same_stream:
            prev = n
            continue
        if start is not None:
            runs.append((start, prev))
        start = prev = n
    if start is not None:
        runs.append((start, prev))
    return runs


def run_path(folder: Path, first: int, last: int) -> str:
    """The manifest path of one run: the folder plus the chunk range it covers."""
    return f"{folder}#chunks={first}-{last}"


def wav_path(wav: Path) -> str:
    return f"{wav}#skip={START_SKIP_S:g}"


def is_field_path(path: str) -> bool:
    if "#" not in path:
        return False
    spec = path.rsplit("#", 1)[1]
    return spec.startswith("chunks=") or spec.startswith("skip=")


def _parse(path: str) -> tuple[Path, dict[str, str]]:
    base, spec = path.rsplit("#", 1)
    fields = dict(kv.split("=", 1) for kv in spec.split(",") if "=" in kv)
    return Path(base), fields


def load_field_audio(path: str) -> tuple[np.ndarray, int]:
    """Audio of a field clip: the chunks of a run concatenated, or a WAV, start transient cut."""
    base, fields = _parse(path)
    skip_s = float(fields.get("skip", 0.0))
    if "chunks" in fields:
        first, last = (int(v) for v in fields["chunks"].split("-", 1))
        parts, sr = [], None
        for n in range(first, last + 1):
            x, r = read_audio(base / f"chunk-{n:03d}.wav")
            if sr is not None and r != sr:
                raise ValueError(f"{base}: chunk {n} is {r} Hz, the run is {sr} Hz")
            sr = r
            parts.append(x)
        if sr is None:
            raise ValueError(f"{path}: empty chunk range")
        x = np.concatenate(parts)
        if first == 1:
            skip_s = max(skip_s, START_SKIP_S)
    else:
        x, sr = read_audio(base)
    return x[int(round(skip_s * sr)) :], sr


def _seconds(path: str) -> tuple[int, float]:
    base, fields = _parse(path)
    skip_s = float(fields.get("skip", 0.0))
    if "chunks" in fields:
        first, last = (int(v) for v in fields["chunks"].split("-", 1))
        total, sr = 0, None
        for n in range(first, last + 1):
            with sf.SoundFile(str(base / f"chunk-{n:03d}.wav")) as f:
                total += len(f)
                sr = f.samplerate
        if first == 1:
            skip_s = max(skip_s, START_SKIP_S)
    else:
        with sf.SoundFile(str(base)) as f:
            total, sr = len(f), f.samplerate
    return int(sr), max(0.0, total / sr - skip_s)


def _items(label_dir: Path) -> Iterator[tuple[str, list[str]]]:
    """(recording name, manifest paths of its clips) for one Positive/Negative directory."""
    for p in sorted(label_dir.iterdir()):
        if p.is_dir():
            runs = chunk_runs(p)
            if runs:
                yield p.name, [run_path(p, a, b) for a, b in runs]
        elif p.suffix.lower() == ".wav":
            yield p.stem, [wav_path(p)]


def field_rows(root: Path, role: str) -> Iterator[dict]:
    """One manifest row per run of every labelled recording under `root` (raw/field)."""
    if not root.exists():
        return
    for session in sorted(p for p in root.iterdir() if p.is_dir()):
        for label_dir in sorted(p for p in session.iterdir() if p.is_dir()):
            label = LABEL_DIRS.get(label_dir.name.lower())
            if label is None:
                continue
            for name, paths in _items(label_dir):
                group = f"{SOURCE}/{session.name}/{name}"
                category = drone_of(name) if label == 1 else name
                for k, path in enumerate(paths):
                    sr, dur = _seconds(path)
                    suffix = f"/run{k + 1}" if len(paths) > 1 else ""
                    yield {
                        "id": f"{group}{suffix}",
                        "source": SOURCE,
                        "path": path,
                        "label": label,
                        "category": category,
                        "group": group,
                        "split": SPLIT,
                        "role": role,
                        "sr": sr,
                        "duration": dur,
                    }


def assign_folds(groups: dict[str, str], k: int) -> dict[str, int]:
    """Spread recordings over `k` folds, each stratum (drone, or "negative") dealt in turn.

    `groups` maps a group to its stratum. Within a stratum the order is a hash
    of the group - deterministic, and blind to the naming, which tends to put
    the takes of one scenario next to each other - and the dealing continues
    across strata so that no fold ends up with every leftover. The same
    recordings always land in the same folds; adding a recording can move
    others, which is why a run stores the folds it was trained with.
    """
    if k < 2:
        raise ValueError("need at least two folds")
    folds: dict[str, int] = {}
    at = 0
    for stratum in sorted(set(groups.values())):
        members = sorted(
            (g for g, s in groups.items() if s == stratum),
            key=lambda g: hashlib.sha1(g.encode("utf-8")).hexdigest(),
        )
        for g in members:
            folds[g] = at % k
            at += 1
    return folds

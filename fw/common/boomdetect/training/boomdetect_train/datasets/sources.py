"""One function per dataset: enumerate its clips as manifest rows.

Provenance, because a manifest that cannot say where a clip came from cannot
say what "unseen" means:

hf_drone_audio    HF `geronimobasso/drone-audio-detection-samples`, 39 parquet
                  shards of 0.5..1 s clips at 16 kHz. Shard 38 is all drones,
                  shard 03 was the noise source of the v1..v6 models. No
                  recording key survives in the file names, so every clip is
                  its own group.
drone_audio_dataset  Al-Emadi et al. 2019 (github saraalemadi/DroneAudioDataset):
                  Parrot Bebop and Mambo recorded indoors, 1 s chunks named
                  "<recording>-<type>_<n>_.wav". Its "unknown" class is cut
                  from ESC-50, so it is NOT enumerated here - ESC-50 itself is,
                  which avoids counting the same audio twice.
esc50             Piczak 2015 (github karolpiczak/ESC-50): 2000 five-second
                  clips in 50 categories, 44.1 kHz. All negatives; the category
                  is kept so a report can say which sounds fool a model
                  (helicopter, chainsaw, engine, ...). ESC-50's own `fold`
                  column is the group.
halmstad          Svanström et al., Drone-detection-dataset: 30 DRONE, 30
                  BACKGROUND, 30 HELICOPTER clips of 10 s. Unseen.
salford           DroneNoise DB flyovers (Ed_*) and two calibration
                  recordings (Calib_*), 50 kHz float. Unseen.
own_recordings    The node's own microphone: playback of drone loops through a
                  speaker (positives, with the caveat that a speaker is not a
                  rotor) and room backgrounds. Evaluation only.
playback_source   The two 16 kHz loops that were played back. Same provenance
                  as the training drones, so an anchor, not evidence.
"""

from __future__ import annotations

import io
from collections.abc import Iterable, Iterator
from pathlib import Path

import pandas as pd
import pyarrow.parquet as pq
import soundfile as sf

from boomdetect_train.datasets.manifest import (
    COLUMNS,
    ROLE_REAL,
    ROLE_STRESS,
    ROLE_TRAIN,
    ROLE_UNSEEN,
    split_for_group,
)
from boomdetect_train.paths import LEGACY_DATA_DIR, raw_dir

HF_REPO = "geronimobasso/drone-audio-detection-samples"
HF_URL = f"https://huggingface.co/datasets/{HF_REPO}/resolve/main/data/"
DRONE_AUDIO_DATASET_URL = "https://github.com/saraalemadi/DroneAudioDataset.git"
ESC50_URL = "https://github.com/karolpiczak/ESC-50.git"


def _probe(path: Path) -> tuple[int, float]:
    with sf.SoundFile(str(path)) as f:
        return f.samplerate, len(f) / f.samplerate


def _row(**kw) -> dict:
    row = {c: kw.get(c) for c in COLUMNS}
    return row


# --- public training data ---------------------------------------------------


def hf_shard_rows(shard: Path, source: str = "hf_drone_audio") -> Iterator[dict]:
    """Every clip of one parquet shard. Sample rate and length come from the WAV header."""
    pf = pq.ParquetFile(shard)
    row_index = 0
    for rg in range(pf.num_row_groups):
        table = pf.read_row_group(rg, columns=["audio", "label"])
        for rec in table.to_pylist():
            audio = rec["audio"]
            with sf.SoundFile(io.BytesIO(audio["bytes"])) as f:
                sr, dur = f.samplerate, len(f) / f.samplerate
            name = audio.get("path") or f"row{row_index}"
            label = int(rec["label"])
            cid = f"{source}/{shard.stem}/{Path(name).stem}"
            group = cid  # no recording key survives; every clip is its own group
            yield _row(
                id=cid,
                source=source,
                path=f"{shard}#{row_index}",
                label=label,
                category="drone" if label == 1 else "noise",
                group=group,
                split=split_for_group(group),
                role=ROLE_TRAIN,
                sr=sr,
                duration=dur,
            )
            row_index += 1


def hf_shards(paths: Iterable[Path]) -> Iterator[dict]:
    for p in paths:
        yield from hf_shard_rows(Path(p))


def drone_audio_dataset_rows(root: Path | None = None) -> Iterator[dict]:
    """The Bebop and Mambo positives, typed by their Multiclass folder."""
    root = root if root is not None else raw_dir() / "DroneAudioDataset"
    for kind, folder in (("bebop", "bebop_1"), ("mambo", "membo_1")):
        d = root / "Multiclass_Drone_Audio" / folder
        for p in sorted(d.glob("*.wav")):
            sr, dur = _probe(p)
            recording = p.stem.split("-")[0]  # "B_S2_D1_067-bebop_000_" -> "B_S2_D1_067"
            group = f"drone_audio_dataset/{recording}"
            yield _row(
                id=f"drone_audio_dataset/{kind}/{p.stem}",
                source="drone_audio_dataset",
                path=str(p),
                label=1,
                category=kind,
                group=group,
                split=split_for_group(group),
                role=ROLE_TRAIN,
                sr=sr,
                duration=dur,
            )


def esc50_rows(root: Path | None = None) -> Iterator[dict]:
    """All 2000 ESC-50 clips as negatives, grouped by ESC-50's own fold."""
    root = root if root is not None else raw_dir() / "ESC-50"
    meta = pd.read_csv(root / "meta" / "esc50.csv")
    for rec in meta.itertuples(index=False):
        p = root / "audio" / rec.filename
        if not p.exists():
            continue
        sr, dur = _probe(p)
        # The same source recording was cut into several clips ("1-100032-A-0"
        # and "1-100032-A-1" share the 100032); that number is the group.
        clip_src = rec.filename.split("-")[1]
        group = f"esc50/{clip_src}"
        yield _row(
            id=f"esc50/{Path(rec.filename).stem}",
            source="esc50",
            path=str(p),
            label=0,
            category=rec.category,
            group=group,
            split=split_for_group(group),
            role=ROLE_TRAIN,
            sr=sr,
            duration=dur,
        )


# --- unseen public data -------------------------------------------------------


def halmstad_rows(root: Path | None = None) -> Iterator[dict]:
    root = root if root is not None else LEGACY_DATA_DIR / "samples" / "Halmstad"
    for p in sorted(root.glob("*.wav")):
        cls = p.stem.split("_")[0].lower()  # DRONE / BACKGROUND / HELICOPTER
        sr, dur = _probe(p)
        yield _row(
            id=f"halmstad/{p.stem}",
            source="halmstad",
            path=str(p),
            label=1 if cls == "drone" else 0,
            category=cls,
            group=f"halmstad/{p.stem}",
            split="unseen",
            role=ROLE_UNSEEN,
            sr=sr,
            duration=dur,
        )


def salford_rows(root: Path | None = None) -> Iterator[dict]:
    root = root if root is not None else LEGACY_DATA_DIR / "samples" / "Salford"
    for p in sorted(root.glob("*.wav")):
        is_drone = p.stem.startswith("Ed_")
        sr, dur = _probe(p)
        yield _row(
            id=f"salford/{p.stem}",
            source="salford",
            path=str(p),
            label=1 if is_drone else 0,
            category="flyover" if is_drone else "calibration",
            group=f"salford/{p.stem}",
            split="unseen",
            role=ROLE_UNSEEN,
            sr=sr,
            duration=dur,
        )


# --- the node's own microphone ------------------------------------------------


def own_recording_rows(root: Path | None = None) -> Iterator[dict]:
    root = root if root is not None else LEGACY_DATA_DIR / "recordings"
    for sub, label, category in (("background", 0, "background"), ("playback", 1, "playback")):
        for p in sorted((root / sub).glob("*.wav")):
            sr, dur = _probe(p)
            yield _row(
                id=f"own/{sub}/{p.stem}",
                source="own_recordings",
                path=str(p),
                label=label,
                category=category,
                group=f"own/{sub}/{p.stem}",
                split="eval_real",
                role=ROLE_REAL,
                sr=sr,
                duration=dur,
            )


def playback_source_rows(root: Path | None = None) -> Iterator[dict]:
    root = root if root is not None else LEGACY_DATA_DIR / "playback_source"
    for p in sorted(root.glob("*.wav")):
        sr, dur = _probe(p)
        kind = "bebop" if "bebop" in p.stem else "mambo" if "membo" in p.stem else "drone"
        yield _row(
            id=f"playback_source/{p.stem}",
            source="playback_source",
            path=str(p),
            label=1,
            category=kind,
            group=f"playback_source/{p.stem}",
            split="eval_real",
            role=ROLE_REAL,
            sr=sr,
            duration=dur,
        )


def stress_rows(root: Path | None = None) -> Iterator[dict]:
    """The synthetic probes from stress.py, if they have been generated."""
    from boomdetect_train.stress import category_of

    root = root if root is not None else raw_dir() / "stress"
    if not root.exists():
        return
    for p in sorted(root.glob("*.wav")):
        sr, dur = _probe(p)
        yield _row(
            id=f"stress/{p.stem}",
            source="stress",
            path=str(p),
            label=0,
            category=category_of(p.stem),
            group=f"stress/{p.stem}",
            split="stress",
            role=ROLE_STRESS,
            sr=sr,
            duration=dur,
        )


def default_hf_shards() -> list[Path]:
    """Every HF shard present locally, in the two places they have lived."""
    found: list[Path] = []
    for d in (raw_dir() / "hf-drone-audio-detection-samples", Path.home() / "Documents" / "drony"):
        if d.exists():
            found.extend(sorted(d.glob("train-*-of-00039.parquet")))
    # Deduplicate by shard name; the same file may exist in both places.
    seen: dict[str, Path] = {}
    for p in found:
        seen.setdefault(p.name, p)
    return list(seen.values())


def build_rows(hf_paths: Iterable[Path] | None = None) -> pd.DataFrame:
    """Enumerate every source that exists on this machine."""
    rows: list[dict] = []
    shards = list(hf_paths) if hf_paths is not None else default_hf_shards()
    rows.extend(hf_shards(shards))
    if (raw_dir() / "DroneAudioDataset").exists():
        rows.extend(drone_audio_dataset_rows())
    if (raw_dir() / "ESC-50" / "meta" / "esc50.csv").exists():
        rows.extend(esc50_rows())
    rows.extend(halmstad_rows())
    rows.extend(salford_rows())
    rows.extend(own_recording_rows())
    rows.extend(playback_source_rows())
    rows.extend(stress_rows())
    return pd.DataFrame(rows, columns=COLUMNS)

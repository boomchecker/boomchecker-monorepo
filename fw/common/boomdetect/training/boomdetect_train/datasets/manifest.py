"""The clip table.

Columns
    id        stable string, unique across sources ("esc50/1-100032-A-0")
    source    which dataset (see sources.py)
    path      file path, or "<parquet path>#<row>" for a clip inside a shard
    label     1 drone, 0 not a drone
    category  finer class: drone type, ESC-50 category, "background", ...
    group     leakage key: clips from the same recording share a group and
              never straddle a split
    split     train / val / unseen / eval_real / stress
    role      what the clip is for (see ROLES) - a label alone does not say
              whether a clip may be trained on
    sr        native sample rate
    duration  seconds at the native rate

Splits are assigned once, deterministically from the group key, so a rebuilt
manifest reproduces the same partition and no clip silently changes sides.
"""

from __future__ import annotations

import hashlib
from pathlib import Path

import pandas as pd

from boomdetect_train.paths import data_root

COLUMNS = ["id", "source", "path", "label", "category", "group", "split", "role", "sr", "duration"]

# Roles say what a clip may be used for. Public training data trains; the two
# unseen public sets and the node's own recordings only ever evaluate; the
# synthetic stress clips exist to probe one failure mode and would teach a model
# nothing real.
ROLE_TRAIN = "train"  # may be trained on (its split decides train vs val)
ROLE_UNSEEN = "unseen"  # public data the models never see in training
ROLE_REAL = "real_mic"  # recorded with the node's own microphone chain
ROLE_STRESS = "stress"  # synthetic probes
ROLES = (ROLE_TRAIN, ROLE_UNSEEN, ROLE_REAL, ROLE_STRESS)

VAL_FRACTION = 0.2


def split_for_group(group: str, val_fraction: float = VAL_FRACTION) -> str:
    """train or val, decided by a hash of the group key so it never moves."""
    h = int.from_bytes(hashlib.sha1(group.encode("utf-8")).digest()[:4], "big")
    return "val" if (h / 2**32) < val_fraction else "train"


def manifest_path() -> Path:
    return data_root() / "manifest.parquet"


def save_manifest(df: pd.DataFrame, path: Path | None = None) -> Path:
    p = path if path is not None else manifest_path()
    p.parent.mkdir(parents=True, exist_ok=True)
    missing = [c for c in COLUMNS if c not in df.columns]
    if missing:
        raise ValueError(f"manifest is missing columns {missing}")
    if df["id"].duplicated().any():
        dup = df.loc[df["id"].duplicated(), "id"].head(3).tolist()
        raise ValueError(f"manifest has duplicate ids, e.g. {dup}")
    df[COLUMNS].to_parquet(p, index=False)
    return p


def load_manifest(path: Path | None = None) -> pd.DataFrame:
    p = path if path is not None else manifest_path()
    if not p.exists():
        raise FileNotFoundError(f"no manifest at {p}; run `bdtrain manifest` first")
    return pd.read_parquet(p)


def summarize(df: pd.DataFrame) -> pd.DataFrame:
    """Clips and hours per (source, role, split, label): the table a report prints."""
    g = df.groupby(["source", "role", "split", "label"], observed=True)
    out = g.agg(clips=("id", "size"), hours=("duration", lambda s: s.sum() / 3600.0))
    return out.reset_index()

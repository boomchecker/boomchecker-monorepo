"""Where things live: the C package this mirrors, and the data it trains on.

The C sources are found relative to this file, so the tables the front end
reads are always the ones the firmware compiles - not a copy that could drift.
The data root is outside the repository (datasets are gigabytes and never
committed); override it with the BOOMDETECT_DATA environment variable.
"""

from __future__ import annotations

import os
from pathlib import Path

PACKAGE_DIR = Path(__file__).resolve().parent
TRAINING_DIR = PACKAGE_DIR.parent
BOOMDETECT_DIR = TRAINING_DIR.parent  # fw/common/boomdetect
REPO_ROOT = BOOMDETECT_DIR.parents[1].parent  # fw/common -> fw -> repo root

MFCC_TABLES_H = BOOMDETECT_DIR / "src" / "mfcc_tables.h"
MODELS_DIR = BOOMDETECT_DIR / "models"
VECTORS_DIR = BOOMDETECT_DIR / "tests" / "vectors"
SELFTEST_HOST_FIXTURE = VECTORS_DIR / "selftest_host.txt"

# Recordings made with the node's own microphone live next to the old research
# tree; they are untracked data, so their location is a fact about this
# machine rather than about the repository.
LEGACY_DATA_DIR = REPO_ROOT / "fw" / "bom-stmnode" / "drony" / "data"


def data_root() -> Path:
    """Root of the dataset tree (raw downloads, manifests, feature caches)."""
    env = os.environ.get("BOOMDETECT_DATA")
    if env:
        return Path(env).expanduser()
    return Path.home() / "Documents" / "boomdetect-data"


def raw_dir() -> Path:
    return data_root() / "raw"


def cache_dir() -> Path:
    return data_root() / "cache"


def runs_dir() -> Path:
    """Trained models, reports and exports, one directory per run."""
    return data_root() / "runs"

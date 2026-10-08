from pathlib import Path

import numpy as np
import pytest

from beamforming import dads

DATA_DIR = Path(__file__).resolve().parents[2] / "data" / "dads"


@pytest.fixture(scope="session")
def drone_clips() -> list[np.ndarray]:
    """All cached DADS drone clips; fails (no skip) when `task sim:fetch` was not run."""
    clips = dads.list_clips(DATA_DIR)
    return [dads.load_clip(c) for c in clips]


@pytest.fixture
def rng() -> np.random.Generator:
    return np.random.default_rng(12345)


@pytest.fixture(scope="session")
def clip_meta() -> list[dads.Clip]:
    """Manifest entries of the cached DADS clips (no audio decoded)."""
    return dads.list_clips(DATA_DIR)

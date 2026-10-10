"""Shared fixtures: the C package's tables and the LCG signal are read once."""

from __future__ import annotations

import numpy as np
import pytest

from boomdetect_train.dsp.mfcc import Frontend
from boomdetect_train.dsp.selftest import lcg_signal
from boomdetect_train.dsp.tables import load_tables


@pytest.fixture(scope="session")
def tables():
    return load_tables()


@pytest.fixture(scope="session")
def frontend(tables):
    return Frontend(tables)


@pytest.fixture(scope="session")
def lcg48k() -> np.ndarray:
    """The board's fixture signal, 66048 int16 samples at 48 kHz."""
    return lcg_signal()


def assert_close(got, want, rel: float, abs_: float, what: str) -> None:
    """|got - want| <= rel * max(|got|, |want|) + abs_, element-wise, with a report."""
    got = np.asarray(got, dtype=np.float64)
    want = np.asarray(want, dtype=np.float64)
    assert got.shape == want.shape, f"{what}: shape {got.shape} vs {want.shape}"
    scale = np.maximum(np.abs(got), np.abs(want))
    err = np.abs(got - want)
    bad = err > rel * scale + abs_
    if bad.any():
        i = int(np.argmax(err - (rel * scale + abs_)))
        raise AssertionError(
            f"{what}: {int(bad.sum())} of {bad.size} values outside tolerance "
            f"(rel {rel}, abs {abs_}); worst at {i}: got {got.flat[i]!r}, "
            f"want {want.flat[i]!r}, err {err.flat[i]:.3g}"
        )

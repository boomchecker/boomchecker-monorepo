"""The Python front end against the C, on the board's own fixture signal.

tests/vectors/selftest_host.txt is what boomdetect_selftest_tool prints on the
Linux CI host: MFCC coefficients of the first three frames, the 52 aggregated
features of all three windows, and each window's mlp_v6 decision, as raw
float32 bit patterns. This drives the Python chain from the same integer LCG
and compares stage by stage.

Tolerances are stated, not assumed. The fixture's own header measures the
board-versus-host gap at 1.8e-4 relative on the smallest MFCC coefficient and
1.1e-6 on a decision; Python (float64 FFT, no RMS pre-scaling) sits within the
same order. The absolute floor matters for the near-zero coefficients, where a
relative bound alone would report noise.
"""

from __future__ import annotations

import numpy as np
import pytest

from boomdetect_train.dsp.audio import decimate3, pcm16_to_float
from boomdetect_train.dsp.selftest import (
    SELFTEST_INPUT_LEN,
    SELFTEST_SEED,
    fnv1a,
    parse_fixture,
)
from boomdetect_train.dsp.windows import Gate, windows
from boomdetect_train.features import LAYOUT_STATS, extract
from boomdetect_train.models.headers import shipped_models
from boomdetect_train.paths import SELFTEST_HOST_FIXTURE
from tests.conftest import assert_close

MFCC_REL, MFCC_ABS = 1e-4, 2e-5
FEAT_REL, FEAT_ABS = 1e-4, 2e-5
DEC_REL, DEC_ABS = 1e-5, 1e-5


@pytest.fixture(scope="module")
def fixture():
    return parse_fixture(SELFTEST_HOST_FIXTURE)


@pytest.fixture(scope="module")
def run(frontend, lcg48k):
    x = pcm16_to_float(decimate3(lcg48k))
    frames = frontend.process(x)
    wins = windows(frames.rms, Gate.PER_FRAME, squelch=0.0)
    return frames, wins


def test_signature_matches_the_board(lcg48k, fixture):
    """Same input first: if this fails nothing downstream is comparable."""
    assert lcg48k.shape[0] == SELFTEST_INPUT_LEN
    sig = f"DSTSIG n={SELFTEST_INPUT_LEN} seed={SELFTEST_SEED} fnv={fnv1a(lcg48k):08X}"
    assert fixture["sig"] == sig


def test_frame_and_window_counts(run):
    frames, wins = run
    assert frames.n == 42
    assert len(wins) == 3
    assert [w.start for w in wins] == [0, 14, 28]


def test_mfcc_frames_match(run, fixture):
    frames, _ = run
    want = fixture["mfcc"]
    assert_close(frames.mfcc[: want.shape[0]], want, MFCC_REL, MFCC_ABS, "MFCC")


def test_aggregated_features_match(run, fixture):
    frames, wins = run
    for w, win in enumerate(wins):
        got = extract(LAYOUT_STATS, frames, win.frames)
        want = np.concatenate([fixture["feat"][w][k] for k in ("mean", "std", "dmea", "cmax")])
        assert_close(got, want, FEAT_REL, FEAT_ABS, f"features of window {w}")


def test_mlp_v6_decisions_match(run, fixture):
    frames, wins = run
    mlp = shipped_models()["mlp_v6"]
    for w, win in enumerate(wins):
        feats = extract(LAYOUT_STATS, frames, win.frames)
        got = float(mlp.score(feats))
        assert_close(got, fixture["dec"][w], DEC_REL, DEC_ABS, f"decision of window {w}")


def test_mlp_v6_decisions_from_the_c_features_match_tighter(fixture):
    """Isolate the classifier: feed it the C's own feature bits."""
    mlp = shipped_models()["mlp_v6"]
    for w, dec in fixture["dec"].items():
        feats = np.concatenate([fixture["feat"][w][k] for k in ("mean", "std", "dmea", "cmax")])
        got = float(mlp.score(feats))
        assert_close(got, dec, 2e-6, 1e-6, f"classifier on C features, window {w}")


def test_svm_v3_runs_and_differs_from_mlp(run):
    frames, wins = run
    models = shipped_models()
    feats = extract(LAYOUT_STATS, frames, wins[0].frames)
    svm = float(models["svm_v3"].score(feats))
    mlp = float(models["mlp_v6"].score(feats))
    assert np.isfinite(svm) and np.isfinite(mlp)
    assert svm != mlp

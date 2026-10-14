"""Missing (NaN) features train and score as the training mean, for every sklearn family."""

from __future__ import annotations

import numpy as np
import pytest

from boomdetect_train import features as F
from boomdetect_train.train import WindowSet, train_gbt, train_mlp, train_svm


def _window_set(seed: int = 0, n: int = 600) -> WindowSet:
    rng = np.random.default_rng(seed)
    width = F.LAYOUTS[F.LAYOUT_STATS_SPECTRAL_MOD].n_features
    y = (np.arange(n) % 2).astype(int)
    x = rng.normal(size=(n, width)).astype(np.float32)
    x[:, 5] += 2.0 * y  # something to learn
    mod = slice(F.N_STATS_SPECTRAL, width)
    x[:, mod] += 0.5 * y[:, None]
    x[: n // 3, mod] = np.nan  # a third of the windows had no full ring
    clip = np.array([f"c{i // 10}" for i in range(n)])
    return WindowSet(x, y, clip, F.LAYOUT_STATS_SPECTRAL_MOD, np.array(["t"] * n))


@pytest.mark.parametrize(
    "trainer",
    [
        lambda ws: train_gbt(ws, max_iter=20),
        lambda ws: train_mlp(ws, hidden=(8,)),
        lambda ws: train_svm(ws),
    ],
    ids=["gbt", "mlp", "svm"],
)
def test_nan_scores_as_the_training_mean(trainer):
    ws = _window_set()
    sm = trainer(ws)
    assert sm.impute is not None and sm.impute.shape == (sm.n_features,)
    off = sm.offset
    full = ws.x[np.isfinite(ws.x).all(axis=1)][:5].copy()  # rows with every feature present
    assert np.isfinite(full).all()
    with_nan = full.copy()
    with_nan[:, F.N_STATS_SPECTRAL :] = np.nan
    filled = full.copy()
    filled[:, F.N_STATS_SPECTRAL :] = sm.impute[F.N_STATS_SPECTRAL - off :]
    np.testing.assert_allclose(sm.score(with_nan), sm.score(filled), rtol=1e-5, atol=1e-5)
    assert np.isfinite(sm.score(with_nan)).all()
    # The fill is the mean of the rows that had the feature.
    col = F.N_STATS_SPECTRAL
    expect = np.nanmean(ws.x[:, col])
    assert sm.impute[col - off] == pytest.approx(expect, rel=1e-5)


def test_models_without_missing_features_carry_no_fill():
    ws = _window_set()
    ws.x[:] = np.nan_to_num(ws.x, nan=0.0)
    sm = train_gbt(ws, max_iter=5)
    assert sm.impute is None

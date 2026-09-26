"""Distance variants: the loss is air's, the floor is synthetic, and nothing is random twice."""

from __future__ import annotations

import numpy as np
import pytest
from scipy import signal

from boomdetect_train.augment import (
    Variant,
    absorb,
    air_loss_db,
    apply_variant,
    draw_variant,
    variants_of,
)

SR = 48000


def _band_db(x: np.ndarray, lo: float, hi: float) -> float:
    f, p = signal.welch(x, SR, nperseg=4096)
    return 10 * np.log10(p[(f >= lo) & (f < hi)].mean())


def test_air_takes_the_top_and_leaves_the_bottom():
    assert air_loss_db(np.asarray([1000.0]), 100.0)[0] == pytest.approx(0.45, abs=0.01)
    assert air_loss_db(np.asarray([8000.0]), 100.0)[0] == pytest.approx(8.3, abs=0.01)
    rng = np.random.default_rng(1)
    x = rng.standard_normal(4 * SR)
    y = absorb(x, SR, 200.0)
    assert y.shape == x.shape
    # ~1 dB at 500-1500 Hz over 200 m, ~14-16 dB at 7-8 kHz: applied once, not squared.
    assert _band_db(x, 500, 1500) - _band_db(y, 500, 1500) == pytest.approx(1.1, abs=0.6)
    assert _band_db(x, 7000, 7900) - _band_db(y, 7000, 7900) == pytest.approx(15.3, abs=2.0)


def test_a_variant_sinks_the_drone_onto_a_floor():
    rng = np.random.default_rng(2)
    tone = (0.1 * np.sin(2 * np.pi * 1000 * np.arange(2 * SR) / SR)).astype(np.float32)
    v = Variant(distance_m=50.0, gain_db=-20.0, white_dbfs=-66.0, rumble_dbfs=-50.0)
    y = apply_variant(tone, SR, v, rng)
    assert y.dtype == np.float32 and y.shape == tone.shape
    # The 1 kHz tone lost ~20 dB (plus a fraction of a dB of air), the floor is there.
    assert _band_db(tone, 950, 1050) - _band_db(y, 950, 1050) == pytest.approx(20.5, abs=1.5)
    assert _band_db(y, 3000, 6000) > -130.0


def test_negatives_keep_their_level_and_variants_are_reproducible():
    rng = np.random.default_rng(3)
    assert draw_variant(rng, drone=False).gain_db == 0.0
    x = np.random.default_rng(4).standard_normal(SR).astype(np.float32) * 0.01
    a = variants_of("field/s/rec", x, SR, 1, 3)
    b = variants_of("field/s/rec", x, SR, 1, 3)
    assert [vid for vid, _, _ in a] == ["field/s/rec#aug1", "field/s/rec#aug2", "field/s/rec#aug3"]
    for (_, ya, va), (_, yb, vb) in zip(a, b, strict=True):
        assert va == vb
        np.testing.assert_array_equal(ya, yb)
    assert a[0][2] != a[1][2], "each variant draws its own parameters"
    assert (
        20.0 <= min(v.distance_m for _, _, v in a) and max(v.distance_m for _, _, v in a) <= 250.0
    )

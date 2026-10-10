"""The OR pairs' z tables and the synthetic parity vectors the C regenerates."""

import numpy as np
import pytest

from boomdetect_train.export import mod_nan_ranges, synthetic_features
from boomdetect_train.features import LAYOUTS
from boomdetect_train.models.pair import ZTable, pair_layout


def test_synthetic_features_are_the_c_sequence():
    # The first values pin the LCG and the float32 scaling the C test regenerates.
    v = synthetic_features(6, seed=12345, k=0)
    state, want = (12345 * 2654435761 + 1) & 0xFFFFFFFF, []
    for _ in range(6):
        state = (state * 1664525 + 1013904223) & 0xFFFFFFFF
        want.append(np.float32(((state >> 8) & 0xFFFFFF) - 0x800000) * np.float32(2**-23) * 3)
    np.testing.assert_array_equal(v, np.asarray(want, dtype=np.float32))
    x = synthetic_features(4000, seed=7, k=3)
    assert x.dtype == np.float32 and x.min() >= -3.0 and x.max() < 3.0
    assert not np.array_equal(x, synthetic_features(4000, seed=7, k=4))


def test_synthetic_nan_shapes_follow_k():
    a, b = (10, 5), (20, 5)
    assert not np.isnan(synthetic_features(40, 1, 0, a, b)).any()
    one = np.isnan(synthetic_features(40, 1, 1, a, b))
    assert one[10:15].all() and one.sum() == 5
    two = np.isnan(synthetic_features(40, 1, 2, a, b))
    assert two[10:15].all() and two[20:25].all() and two.sum() == 10


def test_mod_nan_ranges_point_at_the_modulation_sets():
    a, b = mod_nan_ranges(703)
    assert a == (14 * 64 + 79, 10) and b == (14 * 64 + 69, 10)
    a, b = mod_nan_ranges(903)
    assert a == (31 * 64 + 79, 10) and b == (31 * 64 + 69, 10)
    a, b = mod_nan_ranges(603)
    assert a == (31 * 64 + 69, 10) and b == (0, 0)
    assert mod_nan_ranges(403) == ((0, 0), (0, 0))


def test_pair_layouts():
    assert pair_layout(703, 4) == (703, 14 * 64, 0)
    assert pair_layout(903, 4) == (903, 31 * 64, 0)
    assert pair_layout(403, 4) == (603, 31 * 64, 0)
    for net, lay in ((703, 2), (8, 4)):
        with pytest.raises(ValueError):
            pair_layout(net, lay)
    # Member A's layout 4 sits where the pair layout's aux starts.
    assert LAYOUTS[603].n_features == 31 * 64 + LAYOUTS[4].n_features


def test_ztable_matches_the_full_count_where_it_is_exact():
    rng = np.random.default_rng(3)
    neg = rng.normal(size=5000).astype(np.float32)
    zt = ZTable.build(neg, k=64)
    n = neg.size
    srt = np.sort(neg)
    probe = np.concatenate([zt.top[:60], zt.top[:60] + 1e-4, rng.normal(size=200)]).astype(
        np.float32
    )
    got = zt(probe)
    n_ge = n - np.searchsorted(srt, probe, side="left")
    full = (-np.log10((n_ge + 1.0) / (n + 1.0))).astype(np.float32)
    exact = (n_ge < zt.k) & (n_ge > 0)
    np.testing.assert_array_equal(got[exact], full[exact])
    # Below the table: held at z[K], never above the true value.
    low = n_ge >= zt.k
    assert np.all(got[low] == zt.z_of(zt.k)) and np.all(got[low] >= full[low])


def test_ztable_continues_above_the_top_and_abstains_on_nan():
    zt = ZTable.build(np.arange(100, dtype=np.float32), k=32)
    top = float(zt.top[0])
    z = zt(np.asarray([top + 1.0, top + 2.0, np.nan], dtype=np.float32))
    assert z[0] > zt.z_of(0) and z[1] > z[0]
    assert np.isclose(z[1] - z[0], zt.gain, rtol=1e-5)
    assert z[2] == -np.inf

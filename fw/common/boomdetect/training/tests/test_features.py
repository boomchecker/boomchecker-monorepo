"""The feature layouts: shapes, invariances, and the batch path equals the spec."""

from __future__ import annotations

import numpy as np
import pytest

from boomdetect_train import features as F
from boomdetect_train.dsp.audio import decimate3, pcm16_to_float


@pytest.fixture(scope="module")
def frames(frontend, lcg48k):
    return frontend.process(pcm16_to_float(decimate3(lcg48k)))


def test_layout_widths():
    assert F.LAYOUTS[F.LAYOUT_STATS].n_features == 52
    assert F.LAYOUTS[F.LAYOUT_STATS_SPECTRAL].n_features == 69
    assert F.LAYOUTS[F.LAYOUT_LOGMEL].n_features == 280
    assert len(F.SCALAR_NAMES) == F.N_SCALARS == 8


def test_stats52_matches_a_direct_computation():
    rng = np.random.default_rng(1)
    m = rng.normal(size=(14, 13)).astype(np.float32)
    got = F.stats52(m)
    assert got.shape == (52,)
    np.testing.assert_allclose(got[:13], m.mean(axis=0), rtol=1e-6)
    np.testing.assert_allclose(got[13:26], m.std(axis=0), rtol=1e-5)  # population std
    np.testing.assert_allclose(got[26:39], np.abs(np.diff(m, axis=0)).mean(axis=0), rtol=1e-5)
    np.testing.assert_allclose(got[39:], m.max(axis=0) - m.mean(axis=0), rtol=1e-5)


def test_batch_scalars_equal_the_per_frame_specification(frames):
    idx = np.arange(0, frames.n, 5)
    batch = F.frame_scalars_batch(frames.mag[idx])
    single = np.stack([F.frame_scalars(frames.mag[i]) for i in idx])
    np.testing.assert_allclose(batch, single, rtol=1e-6, atol=1e-7)


def test_scalars_are_bounded_and_finite(frames):
    s = F.frame_scalars_batch(frames.mag)
    assert np.isfinite(s).all()
    hi, mid, centroid, flat, roll, crest, harm, f0 = s.T
    assert ((hi >= 0) & (hi <= 1)).all()
    assert ((mid >= 0) & (mid <= 1)).all()
    assert ((hi + mid) <= 1 + 1e-6).all()
    assert ((centroid > 0) & (centroid < 1)).all()
    assert ((flat >= 0) & (flat <= 1 + 1e-6)).all()
    assert ((roll > 0) & (roll <= 1)).all()
    assert (crest >= 1).all()
    assert ((harm >= 0) & (harm <= 1)).all()
    assert ((f0 >= 60 / 400) & (f0 <= 1.0)).all()


def test_scalars_are_level_invariant(frames):
    a = F.frame_scalars_batch(frames.mag)
    b = F.frame_scalars_batch(frames.mag * 37.0)
    np.testing.assert_allclose(a, b, rtol=1e-5, atol=1e-6)


def test_silent_frame_is_defined():
    s = F.frame_scalars(np.zeros(513))
    assert np.isfinite(s).all()
    assert s[6] == 0.0  # no comb in silence


def test_harmonic_comb_finds_a_synthetic_pitch(frontend):
    sr, f0 = 16000, 180.0
    t = np.arange(sr) / sr
    x = sum(np.sin(2 * np.pi * f0 * h * t) / h for h in range(1, 7)).astype(np.float32) * 0.1
    fr = frontend.process(x)
    s = F.frame_scalars_batch(fr.mag)
    est = s[:, 7].mean() * F.F0_MAX_HZ
    assert abs(est - f0) / f0 < 0.03, est
    # Hamming leakage spreads the peaks, so the comb collects under half of the
    # total magnitude even for a pure harmonic tone; noise sits far below 0.3.
    assert s[:, 6].mean() > 0.3


def test_hum_versus_broadband_differ_in_high_band(frontend):
    """A closed-mouth hum has almost nothing above 4 kHz; rotor noise does."""
    sr = 16000
    rng = np.random.default_rng(0)
    t = np.arange(sr) / sr
    hum = sum(np.sin(2 * np.pi * 140 * h * t) / h**1.5 for h in range(1, 12)) * 0.1
    broad = rng.normal(size=sr) * 0.05 + hum
    s_hum = F.frame_scalars_batch(frontend.process(hum.astype(np.float32)).mag)
    s_broad = F.frame_scalars_batch(frontend.process(broad.astype(np.float32)).mag)
    assert s_hum[:, 0].mean() < 0.02
    assert s_broad[:, 0].mean() > 0.1
    assert s_broad[:, 3].mean() > s_hum[:, 3].mean()  # flatter


def test_stats_spectral_layout_and_cache_path_agree(frames):
    idx = np.arange(14)
    direct = F.stats_spectral(frames.mfcc[idx], frames.mag[idx], frames.logmel[idx])
    scal = F.frame_scalars_batch(frames.mag[idx])
    via_cache = F.stats_spectral_from_scalars(frames.mfcc[idx], scal, frames.logmel[idx])
    assert direct.shape == (69,)
    np.testing.assert_array_equal(direct[:52], F.stats52(frames.mfcc[idx]))
    np.testing.assert_allclose(direct, via_cache, rtol=1e-6)


def test_logmel_patch_is_zero_mean_and_frame_major(frames):
    idx = np.arange(14)
    patch = F.logmel_patch(frames.logmel[idx])
    assert patch.shape == (280,)
    assert abs(float(patch.mean())) < 1e-4
    np.testing.assert_allclose(
        patch.reshape(14, 20), frames.logmel[idx] - frames.logmel[idx].mean(), rtol=1e-5
    )
    with pytest.raises(ValueError):
        F.logmel_patch(frames.logmel[:13])


def test_extract_dispatch(frames):
    idx = np.arange(14)
    assert F.extract(F.LAYOUT_STATS, frames, idx).shape == (52,)
    assert F.extract(F.LAYOUT_STATS_SPECTRAL, frames, idx).shape == (69,)
    assert F.extract(F.LAYOUT_LOGMEL, frames, idx).shape == (280,)
    with pytest.raises(ValueError):
        F.extract(99, frames, idx)

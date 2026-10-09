"""Band spectrograms (dsp/spectro.py), their layouts and the networks that read them."""

import numpy as np
import pytest

from boomdetect_train.datasets.cache import CachedFrames, frame_rows
from boomdetect_train.dsp.mfcc import Frontend, n_frames
from boomdetect_train.dsp.spectro import (
    SPEC_BY_NAME,
    SPEC_FE_NAMES,
    SPEC_FRONTENDS,
    SPEC_N_BANDS,
    band_centres_hz,
    band_matrix,
    spec_frames,
)
from boomdetect_train.evaluate import window_features
from boomdetect_train.features import (
    LAYOUT_STATS_SPECTRAL_MOD,
    LAYOUTS,
    N_STATS_SPECTRAL_MOD,
    spec_layout,
    spec_layout_id,
)
from boomdetect_train.train import feature_offset

SR = 16000


def _tone(seconds=3.0, f=440.0, amp=0.05, seed=0):
    t = np.arange(int(seconds * SR)) / SR
    rng = np.random.default_rng(seed)
    return (amp * np.sin(2 * np.pi * f * t) + 0.002 * rng.standard_normal(t.size)).astype(
        np.float32
    )


def test_six_frontends():
    assert SPEC_FE_NAMES == ("mfe1k", "mfe2k", "mfe4k", "gs1k", "gs2k", "gs4k")


@pytest.mark.parametrize("fe", SPEC_FRONTENDS, ids=lambda fe: fe.name)
def test_band_matrices_are_normalised(fe):
    w = band_matrix(fe)
    assert w.shape == (SPEC_N_BANDS, fe.n_fft // 2 + 1)
    assert np.allclose(w.sum(axis=1), 1.0)
    assert (w >= 0).all() and (w[:, 0] == 0).all()
    c = band_centres_hz(fe)
    assert np.all(np.diff(c) > 0) and 40 < c[0] < 120 and 7000 < c[-1] <= 8000.01


def test_frames_match_the_frame_cache():
    for n in (1024, 1500, 8000, 8001, 47999):
        x = _tone(n / SR)
        specs = spec_frames(x)
        assert {k: v.shape[0] for k, v in specs.items()} == dict.fromkeys(
            SPEC_FE_NAMES, n_frames(n)
        )


def test_long_window_ends_with_the_board_frame():
    """A click in the middle of frame f shows in frame f for every FFT length, never in f - 1."""
    x = np.zeros(SR * 2, np.float32)
    f = 20
    x[512 * f + 512] = 1.0  # frame f covers [512 f + 1024 - N, 512 f + 1024)
    for name in SPEC_FE_NAMES:
        s = spec_frames(x, [name])[name]
        assert s[f].mean() > s[f - 1].mean() + 5, name


def test_tone_lands_in_its_band():
    x = _tone(f=1000.0)
    for fe in SPEC_FRONTENDS:
        s = spec_frames(x, [fe.name])[fe.name]
        peak = int(np.argmax(s[5:].mean(axis=0)))
        assert abs(band_centres_hz(fe)[peak] - 1000.0) < 120, fe.name


def _dip(x, name, f1, f2):
    """How far the band between two lines sits under the weaker line (dB-ish, natural log)."""
    s = spec_frames(x, [name])[name][8:].mean(axis=0)
    c = band_centres_hz(SPEC_BY_NAME[name])
    a, b, mid = (int(np.argmin(np.abs(c - f))) for f in (f1, f2, (f1 + f2) / 2))
    return min(s[a], s[b]) - s[mid]


def test_low_line_resolution():
    """Lines 88 Hz apart (the DJI's comb): every front-end separates them, GS more, and a
    longer FFT helps GS; 30 Hz apart nothing does - the gammatone's own ERB width (~40 Hz
    at 150 Hz), not the FFT, is the limit there."""
    t = np.arange(3 * SR) / SR

    def two(f1, f2):
        return (0.05 * np.sin(2 * np.pi * f1 * t) + 0.05 * np.sin(2 * np.pi * f2 * t)).astype(
            np.float32
        )

    comb = two(88, 176)
    assert _dip(comb, "gs1k", 88, 176) > _dip(comb, "mfe1k", 88, 176) > 0.5
    assert _dip(comb, "gs4k", 88, 176) > _dip(comb, "gs1k", 88, 176) + 0.3
    close = two(140, 170)
    assert all(_dip(close, n, 140, 170) < 0 for n in SPEC_FE_NAMES)


def test_short_clip_history_is_mirrored_not_zero():
    x = _tone(0.5)
    s = spec_frames(x, ["mfe4k"])["mfe4k"]
    # zero padding would make the first frames 6+ dB quieter than the rest
    assert abs(float(s[0].mean() - s[-1].mean())) < 1.0


def test_layout_ids_and_widths():
    assert spec_layout(LAYOUT_STATS_SPECTRAL_MOD) is None
    assert spec_layout(105) == ("patch", "gs4k", 14)
    assert spec_layout(403) == ("patch", "gs1k", 31)
    assert spec_layout(106) is None
    assert spec_layout_id(200, "gs2k") == 204
    assert LAYOUTS[100].n_features == 14 * SPEC_N_BANDS
    assert LAYOUTS[400].n_features == 31 * SPEC_N_BANDS
    assert LAYOUTS[200].n_features == 3 * SPEC_N_BANDS
    assert LAYOUTS[300].n_features == N_STATS_SPECTRAL_MOD + 3 * SPEC_N_BANDS
    assert LAYOUTS[605].n_features == 31 * SPEC_N_BANDS + N_STATS_SPECTRAL_MOD
    assert feature_offset(300) == 1 and feature_offset(200) == 0 and feature_offset(500) == 0


def _cached(x):
    return CachedFrames(frame_rows(Frontend().process(x)), specs=spec_frames(x))


def test_modulation_layouts_7_to_10():
    from boomdetect_train.features import N_MOD, N_MOD_SPEC, N_STATS_SPECTRAL_MOD

    cf = _cached(_tone(9.0))
    idx = np.arange(260, 274)  # past the 8 s ring
    v4, v7, v8 = (window_features(L, cf, idx) for L in (4, 7, 8))
    v9, v10 = (window_features(L, cf, idx) for L in (9, 10))
    assert v7.shape == (N_STATS_SPECTRAL_MOD,) and v8.shape == (N_STATS_SPECTRAL_MOD + N_MOD,)
    assert v9.shape == (N_STATS_SPECTRAL_MOD + N_MOD,)
    assert v10.shape == (N_STATS_SPECTRAL_MOD + N_MOD_SPEC,) and N_MOD_SPEC == 250
    np.testing.assert_array_equal(v8[:N_STATS_SPECTRAL_MOD], v4)  # 2 s set first
    np.testing.assert_array_equal(v9[:N_STATS_SPECTRAL_MOD], v4)
    np.testing.assert_array_equal(v10[:N_STATS_SPECTRAL_MOD], v4)
    assert np.isfinite(v7).all() and np.isfinite(v9).all() and np.isfinite(v10).all()
    early = window_features(9, cf, np.arange(70, 84))  # 2 s ring full, 8 s ring not
    assert np.isfinite(early[:N_STATS_SPECTRAL_MOD]).all() and np.isnan(early[-N_MOD:]).all()


@pytest.mark.parametrize("layout", [100, 205, 302, 404, 501, 603, 703, 803])
def test_window_features_widths(layout):
    cf = _cached(_tone(3.0))
    idx = np.arange(70, 84)
    v = window_features(layout, cf, idx)
    assert v.shape == (LAYOUTS[layout].n_features,)
    kind, fe, t = spec_layout(layout)
    if kind in ("patch", "hybrid"):
        patch = v[: t * SPEC_N_BANDS]
        assert abs(float(patch.mean())) < 1e-4  # level removed
    if kind == "modbstat":
        np.testing.assert_array_equal(
            v[:N_STATS_SPECTRAL_MOD], window_features(LAYOUT_STATS_SPECTRAL_MOD, cf, idx)
        )
    if kind.startswith("hybrid"):
        from boomdetect_train.features import HYBRID_AUX_LAYOUT

        aux = window_features(HYBRID_AUX_LAYOUT[kind], cf, idx)
        np.testing.assert_array_equal(v[-aux.shape[0] :], aux)


def test_level_invariance():
    x = _tone(3.0)
    idx = np.arange(70, 84)
    a, b = _cached(x), _cached(x * 4.0)
    for layout in (100, 203, 405):
        np.testing.assert_allclose(
            window_features(layout, a, idx), window_features(layout, b, idx), atol=2e-3
        )


def test_long_patch_early_window_is_mirrored():
    cf = _cached(_tone(0.5))
    v = window_features(400, cf, np.arange(0, 14))
    assert v.shape == (31 * SPEC_N_BANDS,) and np.isfinite(v).all()


@pytest.mark.parametrize("arch", ["cnn_m", "crnn1d", "crnn2d", "lstm20"])
@pytest.mark.parametrize("layout", [100, 500, 403, 803])
def test_torchnets_train_and_score(arch, layout):
    pytest.importorskip("torch")
    from boomdetect_train.models.torchnets import (
        load_torchnet,
        save_torchnet,
        train_torchnet,
    )

    rng = np.random.default_rng(1)
    n = 256
    x = rng.standard_normal((n, LAYOUTS[layout].n_features)).astype(np.float32)
    y = (rng.random(n) < 0.5).astype(np.int64)
    x[y == 1, :64] += 1.0
    if spec_layout(layout)[0].startswith("hybrid"):
        x[:10, -5:] = np.nan  # an aux window without its modulation ring
    m = train_torchnet(arch, layout, x, y, epochs=2, log=lambda *_: None)
    assert m.meta["macs"] > 0 and m.meta["params"] > 0
    s = m.score(x)
    assert s.shape == (n,) and np.isfinite(s).all()
    import tempfile
    from pathlib import Path

    with tempfile.TemporaryDirectory() as d:
        save_torchnet(m, Path(d) / "m")
        m2 = load_torchnet(Path(d) / "m")
        np.testing.assert_allclose(m2.score(x), s, rtol=1e-4, atol=1e-4)

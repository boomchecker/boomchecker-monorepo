"""Optional processing choices of :class:`doa.Config` (band guard, SNR bin gain, smoothing)."""

from dataclasses import replace

import numpy as np
import pytest

from beamforming import doa
from beamforming import geometry as g
from beamforming import signals as sg

from .helpers import simulate

MIC = g.make_array("2x8_rot", 0.20, 0.07)


def test_defaults_follow_the_ablation():
    cfg = doa.DEFAULT
    assert (cfg.guard_bins, cfg.bin_weighting, cfg.freq_smooth) == (0, "snr", 0)


def test_snr_gain_needs_bins_above_the_noise_floor():
    X = np.ones((4, 257, 3), dtype=complex)
    with pytest.raises(ValueError, match="no bin above"):
        doa.snr_gain(X, sg.band_bins(), replace(doa.DEFAULT, noise_floor_hz=8100.0))
    low = replace(doa.DEFAULT, noise_floor_hz=1500.0)  # below the band: the band edge is used
    np.testing.assert_allclose(doa.snr_gain(X, sg.band_bins(), low), 0.0, atol=1e-12)


@pytest.mark.parametrize(
    "kwargs",
    [
        {"bin_weighting": "wiener"},
        {"guard_bins": -1},
        {"freq_smooth": -1},
    ],
)
def test_config_rejects_invalid_options(kwargs):
    with pytest.raises(ValueError):
        replace(doa.DEFAULT, **kwargs)


def test_smoothing_averages_neighbour_bins_and_shortens_at_the_edges():
    R = np.arange(1.0, 6.0).reshape(5, 1, 1)
    out = doa._smooth_bins(R, 1)[:, 0, 0]
    np.testing.assert_allclose(out, [1.5, 2.0, 3.0, 4.0, 4.5])
    np.testing.assert_allclose(doa._smooth_bins(R, 9)[:, 0, 0], np.full(5, 3.0))
    assert doa._smooth_bins(R, 0) is R


def test_smoothing_raises_the_covariance_rank_above_the_frame_count(rng):
    X = rng.standard_normal((16, 7, 5)) + 1j * rng.standard_normal((16, 7, 5))
    assert np.linalg.matrix_rank(doa._covariance(X)[3]) == 5
    assert np.linalg.matrix_rank(doa._covariance(X, 1)[3]) == 15


def test_snr_gain_is_zero_for_noise_and_near_one_for_strong_bins(rng):
    noise = rng.standard_normal((16, 257, 5)) + 1j * rng.standard_normal((16, 257, 5))
    X = noise.copy()
    X[:, 20, :] *= 30.0  # one strong bin inside the band
    bins = sg.band_bins()
    gain = doa.snr_gain(X, bins)
    assert gain.shape == bins.shape
    assert gain[20 - bins[0]] > 0.99
    quiet = np.delete(gain, 20 - bins[0])
    assert quiet.max() < 0.2 and quiet.mean() < 0.05


def test_snr_gain_uses_only_bins_above_the_noise_floor_hz():
    X = np.ones((4, 257, 3), dtype=complex)
    X[:, 10:65] *= 3.0  # in-band power 9 against a flat floor of 1
    X[:, 65:96] *= 100.0  # loud bins between the band and the noise floor must not count
    gain = doa.snr_gain(X, sg.band_bins())
    np.testing.assert_allclose(gain, 1.0 - 1.0 / 9.0)


def test_gain_multiplies_the_method_weights(monkeypatch):
    seen = {}

    def bins(X, mic_pos, freqs, dirs, cfg):
        return np.array([np.exp(-0.5 * (g.angular_error_deg(dirs, dirs[0]) / 3.0) ** 2)] * 2)

    monkeypatch.setitem(doa.GRID_METHODS, "flat", doa.GridMethod("flat", bins, True))
    real = doa.combine

    def spy(b, weights=None):
        seen["w"] = weights
        return real(b, weights)

    monkeypatch.setattr(doa, "combine", spy)
    X = np.zeros((16, 2, 5), dtype=complex)
    doa.search("flat", X, MIC, np.ones(2), doa.DEFAULT, gain=np.array([1.0, 0.0]))
    np.testing.assert_allclose(seen["w"][1], 0.0)
    assert seen["w"][0] > 0


def test_guard_bins_drop_the_band_edges(monkeypatch, drone_clips, rng):
    got = {}

    def fake_search(method, X, mic_pos, freqs, cfg=doa.DEFAULT, gain=None):
        got["freqs"] = freqs
        return 0.0, 0.0

    monkeypatch.setattr(doa, "search", fake_search)
    x, _ = simulate(drone_clips[0], MIC, 40.0, 30.0, 20.0, rng)
    doa.localize("das", x, MIC)
    full = got["freqs"]
    doa.localize("das", x, MIC, replace(doa.DEFAULT, guard_bins=2))
    np.testing.assert_allclose(got["freqs"], full[2:-2])
    with pytest.raises(ValueError, match="no bin"):
        doa.localize("das", x, MIC, replace(doa.DEFAULT, guard_bins=40))


@pytest.mark.parametrize("method", ["das", "mvdr", "srp_phat", "music"])
@pytest.mark.parametrize(
    "options",
    [{"guard_bins": 1}, {"bin_weighting": "snr"}, {"freq_smooth": 1}, {"hop": 128}],
)
def test_each_option_still_localises_at_high_snr(method, options, drone_clips, rng):
    cfg = replace(doa.DEFAULT, **options)
    x, u = simulate(drone_clips[3], MIC, 211.3, 38.4, 30.0, rng)
    az, el = doa.localize(method, x, MIC, cfg)
    assert g.angular_error_deg(g.unit_vector(az, el), u) <= 2.0

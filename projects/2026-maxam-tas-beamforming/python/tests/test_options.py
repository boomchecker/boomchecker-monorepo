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


# (azimuth, elevation) in radians from the M3 code (commit 525c3ee) for DAS, MVDR, SRP-PHAT and
# MUSIC on three fixed cases; bin_weighting="max" must reproduce them exactly
M3_ESTIMATES = [
    [
        [0.6298139095045002, 0.40136115799661537],
        [0.6296774444110212, 0.3839109759817981],
        [0.6605091421951368, 0.3837266252787979],
        [0.6296774444110212, 0.3839109759817981],
    ],
    [
        [3.553668025291943, 0.7677978470243935],
        [3.553668025291943, 0.7677978470243935],
        [3.577924966588375, 0.7853981633974482],
        [3.553668025291943, 0.7677978470243935],
    ],
    [
        [5.00999056011561, 0.22675233751024693],
        [5.025652638851784, 0.22675233751024687],
        [5.025652638851784, 0.22675233751024687],
        [5.009852412054861, 0.20931018586616565],
    ],
]
M3_CASES = [(37.3, 22.7, 0.0), (203.8, 41.9, 5.0), (287.2, 11.4, 10.0)]


@pytest.mark.parametrize("case", range(3))
def test_max_weighting_reproduces_the_m3_processing(case, drone_clips):
    from beamforming import dads

    az, el, snr = M3_CASES[case]
    clip = drone_clips[case]
    rng = np.random.default_rng(100 + case)
    u = g.unit_vector(np.deg2rad(az), np.deg2rad(el))
    x = sg.add_noise(sg.observe(clip, MIC, u, dads.random_offset(len(clip), rng)), snr, rng)
    cfg = doa.Config(bin_weighting="max")
    got = [doa.localize(m, x, MIC, cfg) for m in ("das", "mvdr", "srp_phat", "music")]
    np.testing.assert_allclose(got, M3_ESTIMATES[case], rtol=1e-12, atol=1e-12)

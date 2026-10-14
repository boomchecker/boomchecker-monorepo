import numpy as np
import pytest

from beamforming import doa
from beamforming import geometry as g
from beamforming import signals as sg

from .helpers import simulate


def bump_method(monkeypatch, peaks, sigma_deg=2.0, normalize=True):
    """Register a fake grid method: one Gaussian bump per bin, ``peaks = [(az, el, amp), ...]``."""
    centres = [(g.unit_vector(np.deg2rad(a), np.deg2rad(e)), amp) for a, e, amp in peaks]

    def bins(X, mic_pos, freqs, dirs, cfg):
        out = []
        for u0, amp in centres:
            angle = np.deg2rad(g.angular_error_deg(dirs, u0))
            out.append(amp * np.exp(-0.5 * (angle / np.deg2rad(sigma_deg)) ** 2))
        return np.array(out)

    monkeypatch.setitem(doa.GRID_METHODS, "bump", doa.GridMethod("bump", bins, normalize))
    return (
        np.zeros((16, len(peaks), 5), dtype=complex),
        g.make_array("2x8_rot"),
        np.ones(len(peaks)),
    )


def test_fine_stage_reuses_the_coarse_bin_weights(monkeypatch):
    # two bins with equal peak shape but 1000x different amplitude, centres 3 deg apart in
    # elevation. Normalised by the coarse maxima the quiet bin (further from a coarse node)
    # weighs more and the sum peaks near 58 deg; unweighted, the loud bin wins at about 60.5
    X, mic, freqs = bump_method(monkeypatch, [(120.0, 60.5, 1000.0), (120.0, 57.5, 1.0)])
    az, el = doa.search("bump", X, mic, freqs)
    assert 56.5 <= np.rad2deg(el) <= 59.0
    assert abs(np.rad2deg(az) - 120.0) <= 1.0


def test_fine_search_reaches_the_corner_of_a_coarse_cell(monkeypatch):
    # with a 10 deg coarse grid the nearest node can be 5 deg away in both axes, which the
    # default fine cap (+-5 deg) has to cover
    X, mic, freqs = bump_method(monkeypatch, [(125.0, 5.0, 1.0)], sigma_deg=6.0, normalize=False)
    cfg = doa.Config(coarse_step=10.0)
    az, el = doa.search("bump", X, mic, freqs, cfg)
    target = g.unit_vector(np.deg2rad(125.0), np.deg2rad(5.0))
    assert g.angular_error_deg(g.unit_vector(az, el), target) <= 0.8


@pytest.mark.parametrize("method", list(doa.GRID_METHODS))
def test_power_map_is_the_documented_bin_sum(method, drone_clips, rng):
    mic = g.make_array("2x8_rot", 0.20, 0.07)
    x, _ = simulate(drone_clips[2], mic, 130.0, 40.0, 10.0, rng)
    bins = sg.band_bins()
    X, freqs = sg.stft(x)[:, bins], sg.bin_freqs(bins)
    m = doa.GRID_METHODS[method]
    az, el = g.coarse_grid(5.0)
    per_bin = m.bins(X, mic, freqs, g.unit_vector(az, el), doa.DEFAULT)
    if method in ("mvdr", "music"):  # report eqs. (mvdr), (music): sum_k P_k / max P_k
        expected = (per_bin / per_bin.max(axis=1, keepdims=True)).sum(axis=0)
    else:  # DAS and SRP-PHAT: plain sum
        expected = per_bin.sum(axis=0)
    got = doa.power_map(method, X, mic, freqs).ravel()
    np.testing.assert_allclose(got, expected, rtol=1e-9)


def test_localize_matches_bin_data_with_bin_frequencies(rng):
    # a pure tone at the centre of bin 13 (406.25 Hz); data from another bin than the steering
    # frequency scales every delay by 13/12 and moves the elevation by several degrees
    mic = g.make_array("2x8_rot", 0.20, 0.07)
    u = g.unit_vector(np.deg2rad(60), np.deg2rad(45))
    t = np.arange(8000) / sg.FS
    tone = np.sin(2 * np.pi * 13 * sg.FS / sg.NFFT * t)
    x = sg.add_noise(sg.observe(tone, mic, u, 2000), 40.0, rng)
    az, el = doa.localize("das", x, mic)
    assert g.angular_error_deg(g.unit_vector(az, el), u) <= 1.0

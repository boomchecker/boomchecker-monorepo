from dataclasses import replace

import numpy as np
import pytest

from beamforming import doa
from beamforming import geometry as g
from beamforming import signals as sg

from .helpers import band_noise, exact_delays

STEP = 1 / (16 * sg.FS)  # resolution of the zero-padded GCC peak search


def observe(src, mic, u, snr_db, rng):
    return sg.add_noise(sg.observe(src, mic, u, 2000), snr_db, rng)


def test_gcc_delays_match_the_exact_delays(rng):
    mic = g.make_array("2x8_rot", 0.20, 0.07)
    u = g.unit_vector(np.deg2rad(40), np.deg2rad(25))
    diff, tau = doa.gcc_delays(observe(band_noise(rng), mic, u, 30.0, rng), mic)
    assert diff.shape == (120, 3)
    assert np.abs(tau - exact_delays(mic, u)[1]).max() <= 2 * STEP


def test_gcc_delays_reach_the_physical_limit_for_end_fire_pairs(rng):
    mic = g.make_array("2x8_rot", 0.20, 0.07)
    u = g.unit_vector(0.0, 0.0)  # along the line of the diametral pairs: tau = 0.2 m / c
    _, tau = doa.gcc_delays(observe(band_noise(rng), mic, u, 30.0, rng), mic)
    expected = exact_delays(mic, u)[1]
    assert np.abs(expected).max() / STEP > 140  # about 149 steps, close to the search window
    assert np.abs(tau - expected).max() <= 2 * STEP


def test_gcc_delays_never_exceed_the_physical_bound(rng):
    mic = g.make_array("2x8_rot", 0.20, 0.07)
    u = g.unit_vector(np.deg2rad(40), np.deg2rad(25))
    x = observe(band_noise(rng), mic, u, -30.0, rng)  # peaks are random, the window still holds
    diff, tau = doa.gcc_delays(x, mic)
    assert np.all(np.abs(tau) <= np.linalg.norm(diff, axis=1) / sg.C + 2 * STEP)


def test_gcc_band_restriction_improves_delays_at_low_snr():
    mic = g.make_array("2x8_rot", 0.20, 0.07)
    u = g.unit_vector(np.deg2rad(40), np.deg2rad(25))
    full = replace(doa.DEFAULT, band=(0.0, sg.FS / 2))
    rms = {"band": [], "full": []}
    for seed in range(8):
        rng = np.random.default_rng(seed)
        x = observe(band_noise(rng), mic, u, 0.0, rng)
        exact = exact_delays(mic, u)[1]
        for name, cfg in (("band", doa.DEFAULT), ("full", full)):
            rms[name].append(np.sqrt(np.mean((doa.gcc_delays(x, mic, cfg)[1] - exact) ** 2)))
    assert np.mean(rms["band"]) < 0.5 * np.mean(rms["full"])


def test_gcc_phat_ignores_a_loud_coherent_interferer(rng):
    mic = g.make_array("2x8_rot", 0.20, 0.07)
    u = g.unit_vector(np.deg2rad(40), np.deg2rad(25))
    u_int = g.unit_vector(np.deg2rad(200), np.deg2rad(10))
    t = np.arange(8000) / sg.FS
    for _ in range(4):
        src = band_noise(rng)
        src /= np.std(src)
        tone = np.sqrt(2) * 10 ** (15 / 20) * np.sin(2 * np.pi * 1200 * t + rng.uniform(0, 6.28))
        x = sg.add_noise(
            sg.observe(src, mic, u, 2000) + sg.observe(tone, mic, u_int, 2000), 30, rng
        )
        az, el = doa.gcc_phat_ls(x, mic)
        # PHAT gives every bin unit weight, so the tone owns a few bins, not the correlation
        assert g.angular_error_deg(g.unit_vector(az, el), u) <= 2.0


@pytest.mark.parametrize("el_deg", [20.0, 40.0, 60.0])
def test_gcc_planar_array_recovers_elevation(el_deg, rng):
    mic = g.make_array("1x8", 0.20)
    u = g.unit_vector(np.deg2rad(100), np.deg2rad(el_deg))
    x = sg.add_noise(sg.observe(band_noise(rng), mic, u, 2000), 30.0, rng)
    az, el = doa.gcc_phat_ls(x, mic)
    assert abs(np.rad2deg(el) - el_deg) <= 2.0
    assert g.azimuth_error_deg(az, np.deg2rad(100)) <= 2.0


@pytest.mark.parametrize("topology", ["1x8", "2x8", "2x8_rot"])
def test_direction_from_exact_delays_is_exact(topology):
    mic = g.make_array(topology, 0.20, 0.07)
    for az, el in [(10, 5), (130, 40), (250, 75), (330, 20)]:
        u = g.unit_vector(np.deg2rad(az), np.deg2rad(el))
        diff, tau = exact_delays(mic, u)
        est = doa.direction_from_delays(diff, tau)
        # a planar array only fixes u_xy; u_z follows from |u| = 1 and must still be exact
        np.testing.assert_allclose(est, u, atol=1e-9)


def test_direction_below_the_horizon_is_clipped_to_it():
    mic = g.make_array("2x8_rot", 0.20, 0.07)
    u = g.unit_vector(np.deg2rad(70), np.deg2rad(-6))
    diff, tau = exact_delays(mic, u)
    est = doa.direction_from_delays(diff, tau)
    assert est[2] == 0.0
    assert np.linalg.norm(est) == pytest.approx(1.0)
    az, _ = g.to_angles(est)
    assert g.azimuth_error_deg(az, np.deg2rad(70)) <= 0.1


def test_planar_solution_outside_the_unit_circle_is_projected():
    mic = g.make_array("1x8", 0.20)
    diff, tau = exact_delays(mic, g.unit_vector(np.deg2rad(30), np.deg2rad(5)))
    est = doa.direction_from_delays(diff, 1.2 * tau)  # |u_xy| slightly above 1
    assert est[2] == 0.0
    assert np.linalg.norm(est) == pytest.approx(1.0)

import numpy as np
import pytest

from beamforming import beampattern as bp
from beamforming import geometry as g
from beamforming import signals as sg

MIC = g.make_array("2x8_rot", 0.20, 0.07)


def test_pattern_is_one_at_the_steering_direction_and_below_one_elsewhere(rng):
    u0 = g.unit_vector(np.deg2rad(70.0), np.deg2rad(40.0))
    dirs = np.vstack([u0, g.random_directions(200, rng)])
    b = bp.power(MIC, 1500.0, u0, dirs)
    assert b[0] == pytest.approx(1.0)
    assert np.all(b[1:] < 1.0 + 1e-12) and np.all(b >= 0.0)


def test_two_microphone_lobe_width_has_the_closed_form():
    # mics at x = +-d/2, steering to the zenith: B = cos^2(pi f d sin(phi) / c) along the x axis
    d, f = 0.1, 1000.0
    mic = np.array([[-d / 2, 0.0, 0.0], [d / 2, 0.0, 0.0]])
    zenith = np.array([0.0, 0.0, 1.0])
    want = 2 * np.rad2deg(np.arcsin(sg.C / (4 * f * d)))
    assert bp.lobe_width_deg(mic, f, zenith, "east") == pytest.approx(want, abs=0.01)
    assert bp.lobe_width_deg(mic, f, zenith, "north") == pytest.approx(360.0)  # no y aperture


def test_width_shrinks_with_frequency():
    widths = [bp.evaluate(MIC, f, azimuths_deg=(0.0, 20.0)) for f in bp.FREQS_HZ[1:]]
    for a, b in zip(widths, widths[1:], strict=False):
        assert a.width_az > b.width_az and a.width_up > b.width_up and a.width_down > b.width_down


def test_planar_array_has_the_mirror_lobe_below_the_horizon_and_the_second_ring_removes_it():
    # steered to 45 deg, the horizon is 45 deg below; a planar array is symmetric in z, so the
    # downward side runs into the mirror main lobe at -45 deg and stays above -3 dB long after
    one = bp.evaluate(g.make_array("1x8", 0.20), 1000.0, azimuths_deg=(0.0,))
    two = bp.evaluate(g.make_array("2x8_rot", 0.20, 0.10), 1000.0, azimuths_deg=(0.0,))
    assert one.width_down > 90.0
    assert two.width_down < 60.0
    assert two.width_up == pytest.approx(one.width_up, rel=0.1)  # above the horizon: alike
    assert two.width_az == pytest.approx(one.width_az, rel=0.05)


def test_half_widths_are_symmetric_at_the_zenith():
    up, down = bp.half_widths_deg(MIC, 1500.0, np.array([0.0, 0.0, 1.0]), "east")
    assert up == pytest.approx(down, abs=0.2)
    assert bp.lobe_width_deg(MIC, 1500.0, np.array([0.0, 0.0, 1.0]), "east") == pytest.approx(
        up + down
    )


def test_low_frequency_is_omnidirectional_without_sidelobes():
    r = bp.evaluate(MIC, 300.0, azimuths_deg=(0.0,))
    assert r.width_az == 360.0 and np.isnan(r.psl_db)


def test_aliasing_gives_a_strong_sidelobe_above_the_alias_frequency():
    low = bp.evaluate(MIC, 1000.0, azimuths_deg=(0.0, 45.0))
    high = bp.evaluate(MIC, 3000.0, azimuths_deg=(0.0, 45.0))
    assert low.psl_db < -9.0
    assert -8.0 < high.psl_db < 0.0


def test_hemisphere_grid_has_no_zenith_row():
    dirs, (n_az, n_el) = bp.hemisphere_grid()
    assert (n_az, n_el) == (360, 90) and len(dirs) == 360 * 90
    assert dirs[:, 2].min() >= 0.0 and dirs[:, 2].max() < 1.0


def gaussian_map(peaks, sigma=4.0, shape=(360, 90)):
    a, e = np.meshgrid(np.arange(shape[0]), np.arange(shape[1]), indexing="ij")
    out = np.zeros(shape)
    for pa, pe, amp in peaks:
        da = np.minimum(np.abs(a - pa), shape[0] - np.abs(a - pa))
        out += amp * np.exp(-0.5 * (da**2 + (e - pe) ** 2) / sigma**2)
    return out


def test_peak_sidelobe_finds_the_second_lobe_and_wraps_in_azimuth():
    b = gaussian_map([(10, 40, 1.0), (359, 20, 0.3)])  # second lobe next to azimuth 0 / 359
    assert bp.peak_sidelobe_db(b, (10, 40)) == pytest.approx(10 * np.log10(0.3), abs=0.05)
    b = gaussian_map([(100, 40, 1.0), (260, 10, 0.1), (200, 70, 0.4)])
    assert bp.peak_sidelobe_db(b, (100, 40)) == pytest.approx(10 * np.log10(0.4), abs=0.05)


def test_peak_sidelobe_is_nan_without_a_second_lobe():
    assert np.isnan(bp.peak_sidelobe_db(gaussian_map([(30, 45, 1.0)]), (30, 45)))

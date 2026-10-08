import numpy as np
import pytest

from beamforming import geometry as g


@pytest.mark.parametrize(("topology", "n_mics"), [("1x8", 8), ("2x8", 16), ("2x8_rot", 16)])
def test_make_array_shape_and_radius(topology, n_mics):
    pos = g.make_array(topology, diameter=0.25, height=0.1)
    assert pos.shape == (n_mics, 3)
    np.testing.assert_allclose(np.hypot(pos[:, 0], pos[:, 1]), 0.125)


def test_2x8_boards_and_rotation():
    pos = g.make_array("2x8_rot", diameter=0.2, height=0.07)
    np.testing.assert_allclose(pos[:8, 2], -0.035)
    np.testing.assert_allclose(pos[8:, 2], 0.035)
    # mic 0 of the lower ring on the x axis, upper ring rotated by 22.5 deg
    np.testing.assert_allclose(pos[0], [0.1, 0.0, -0.035], atol=1e-12)
    az_upper = np.rad2deg(np.arctan2(pos[8, 1], pos[8, 0]))
    assert az_upper == pytest.approx(22.5)
    plain = g.make_array("2x8", diameter=0.2, height=0.07)
    np.testing.assert_allclose(plain[8, :2], plain[0, :2])


def test_unit_vector_round_trip():
    rng = np.random.default_rng(0)
    az = rng.uniform(0, 2 * np.pi, 50)
    el = rng.uniform(0, np.pi / 2 - 1e-3, 50)
    u = g.unit_vector(az, el)
    np.testing.assert_allclose(np.linalg.norm(u, axis=1), 1.0)
    az2, el2 = g.to_angles(u)
    np.testing.assert_allclose(az2, az, atol=1e-12)
    np.testing.assert_allclose(el2, el, atol=1e-12)


def test_unit_vector_convention_matches_report():
    # u = [cos el cos az, cos el sin az, sin el]
    np.testing.assert_allclose(g.unit_vector(0.0, 0.0), [1, 0, 0], atol=1e-15)
    np.testing.assert_allclose(g.unit_vector(np.pi / 2, 0.0), [0, 1, 0], atol=1e-15)
    np.testing.assert_allclose(g.unit_vector(1.0, np.pi / 2), [0, 0, 1], atol=1e-15)


def test_coarse_grid_size():
    az, el = g.coarse_grid(5.0)
    assert az.shape == el.shape == (1368,)
    assert g.grid_shape(5.0) == (72, 19)
    assert np.rad2deg(el.max()) == pytest.approx(90.0)
    assert np.rad2deg(az.max()) == pytest.approx(355.0)


def test_angular_and_azimuth_error():
    a = g.unit_vector(np.deg2rad(10), np.deg2rad(20))
    b = g.unit_vector(np.deg2rad(10), np.deg2rad(25))
    assert g.angular_error_deg(a, b) == pytest.approx(5.0)
    assert g.azimuth_error_deg(np.deg2rad(358), np.deg2rad(2)) == pytest.approx(4.0)


@pytest.mark.parametrize(("az_deg", "el_deg"), [(120, 40), (357, 30), (0, 90), (200, 87), (45, 2)])
def test_fine_cap_spacing_and_hemisphere(az_deg, el_deg):
    u0 = g.unit_vector(np.deg2rad(az_deg), np.deg2rad(el_deg))
    cap = g.fine_cap(u0, span_deg=5, step_deg=1)
    assert cap.shape[0] <= 121
    assert np.all(cap[:, 2] >= 0)
    np.testing.assert_allclose(np.linalg.norm(cap, axis=1), 1.0)
    err = g.angular_error_deg(cap, u0)
    assert err.min() == pytest.approx(0.0, abs=1e-6)
    assert err.max() <= 5 * np.sqrt(2) + 0.1
    if el_deg >= 6:  # far from the horizon the full 11 x 11 cap survives
        assert cap.shape[0] == 121
    # any direction within 4.5 deg of u0 has a cap point closer than 1 deg
    rng = np.random.default_rng(1)
    east, north = g.tangent_basis(u0)
    for _ in range(20):
        ang = rng.uniform(0, 2 * np.pi)
        rho = np.deg2rad(rng.uniform(0, 4.5))
        v = np.cos(rho) * u0 + np.sin(rho) * (np.cos(ang) * east + np.sin(ang) * north)
        if v[2] < 0:
            continue
        assert g.angular_error_deg(cap, v).min() < 1.0


def test_random_directions_uniform_on_hemisphere():
    u = g.random_directions(20000, np.random.default_rng(0))
    np.testing.assert_allclose(np.linalg.norm(u, axis=1), 1.0)
    assert np.all(u[:, 2] >= 0)
    # uniform in area: z is uniform on [0, 1], the horizontal components are zero-mean
    assert np.mean(u[:, 2]) == pytest.approx(0.5, abs=0.01)
    assert np.mean(u[:, 0]) == pytest.approx(0.0, abs=0.02)
    assert np.mean(u[:, 1]) == pytest.approx(0.0, abs=0.02)

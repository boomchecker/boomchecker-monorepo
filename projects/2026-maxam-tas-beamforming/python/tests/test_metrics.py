import numpy as np
import pytest

from beamforming import geometry as g
from beamforming import metrics as mt


def u(az, el):
    return g.unit_vector(np.deg2rad(az), np.deg2rad(el))


def test_band_edges_split_the_hemisphere_into_equal_areas():
    np.testing.assert_allclose(mt.BAND_EDGES_DEG, [0.0, 19.4712, 41.8103, 90.0], atol=1e-3)
    el = np.rad2deg(np.arcsin(np.random.default_rng(0).uniform(0, 1, 30000)))
    counts = np.bincount(mt.elevation_band(el), minlength=3)
    np.testing.assert_allclose(counts / counts.sum(), 1 / 3, atol=0.01)


@pytest.mark.parametrize(
    ("el", "band"), [(0.0, 0), (19.4, 0), (19.6, 1), (41.7, 1), (41.9, 2), (90.0, 2)]
)
def test_elevation_band_boundaries(el, band):
    assert mt.elevation_band(el) == band


def test_errors_weight_azimuth_by_the_cosine_of_the_true_elevation():
    e = mt.errors(u([10.0, 10.0], [60.0, 0.0]), u([0.0, 0.0], [60.0, 0.0]))
    np.testing.assert_allclose(e.azimuth, [10.0 * 0.5, 10.0], atol=1e-9)
    np.testing.assert_allclose(e.elevation, 0.0, atol=1e-9)
    # arc length: the great-circle angle of a 10 deg azimuth step at 60 deg elevation is close
    assert abs(e.angular[0] - e.azimuth[0]) < 0.2


def test_azimuth_error_wraps_and_the_zenith_is_dropped():
    e = mt.errors(
        u([359.0, 20.0, 5.0], [30.0, 87.0, 89.0]), u([2.0, 20.0, 200.0], [30.0, 87.0, 88.0])
    )
    assert e.azimuth[0] == pytest.approx(3.0 * np.cos(np.deg2rad(30.0)))
    assert np.isnan(e.azimuth[1]) and np.isnan(e.azimuth[2])
    assert e.elevation[2] == pytest.approx(1.0)


def test_rmse_ignores_nan_and_empty_is_nan():
    assert mt.rmse(np.array([3.0, np.nan, 4.0])) == pytest.approx(np.sqrt(12.5))
    assert np.isnan(mt.rmse(np.array([np.nan])))
    assert np.isnan(mt.outlier_fraction(np.array([])))


def test_outlier_fraction_is_strictly_above_the_threshold():
    assert mt.outlier_fraction(np.array([1.0, 5.0, 5.1, 20.0])) == 0.5


def test_summarize_splits_by_true_elevation_band():
    el_true = np.array([5.0, 10.0, 30.0, 35.0, 60.0, 80.0])
    est = u(np.zeros(6), el_true + np.array([1.0, 1.0, 2.0, 2.0, 10.0, 10.0]))
    s = mt.summarize(mt.errors(est, u(np.zeros(6), el_true)))
    assert s.n == 6
    np.testing.assert_allclose(s.band_rmse, [1.0, 2.0, 10.0], atol=1e-6)
    np.testing.assert_allclose(s.band_outliers, [0.0, 0.0, 1.0])
    assert s.outliers == pytest.approx(1 / 3)
    assert s.rmse_el == pytest.approx(np.sqrt((1 + 1 + 4 + 4 + 100 + 100) / 6))
    row = s.row()
    assert row["rmse_band2"] == pytest.approx(10.0) and row["n"] == 6.0


def test_summarize_of_an_empty_band_is_nan():
    s = mt.summarize(mt.errors(u([0.0], [80.0]), u([0.0], [80.0])))
    assert np.isnan(s.band_rmse[0]) and s.band_rmse[2] == pytest.approx(0.0, abs=1e-6)


def test_distinguishable_uses_the_smaller_rmse_as_reference():
    assert not mt.distinguishable(1.0, 1.14)
    assert mt.distinguishable(1.0, 1.16)
    assert mt.distinguishable(1.16, 1.0)

import numpy as np
import pytest

from beamforming import experiment as ex
from beamforming import results as rs

GEO_A = ex.Geometry("2x8_rot", 0.20, 0.07)
GEO_B = ex.Geometry("2x8", 0.16, 0.04)
GEO_C = ex.Geometry("1x8", 0.25)


def fake_estimates(n=20, err_deg=1.0, seed=0):
    rng = np.random.default_rng(seed)
    truth = ex.g.random_directions(n, rng)
    noise = rng.normal(0.0, np.deg2rad(err_deg), (n, 2, 2, 3))
    est = truth[:, None, None, :] + noise
    return est / np.linalg.norm(est, axis=-1, keepdims=True), truth


def test_make_rows_has_one_row_per_snr_and_method():
    est, truth = fake_estimates()
    rows = rs.make_rows("screening", GEO_A, "", ("das", "music"), (10.0, 0.0), est, truth)
    assert len(rows) == 4
    assert set(rows[0]) == set(rs.COLUMNS)
    first = rows[0]
    assert (first["topology"], first["diameter_mm"], first["height_mm"]) == ("2x8_rot", "200", "70")
    assert (first["method"], first["snr_db"], first["n"]) == ("das", "10", "20.0000")
    assert 0.5 < float(first["rmse"]) < 3.0


def test_crb_rows_take_the_rms_over_trials():
    bounds = np.array([[[3.0, 0.0, 3.0]], [[4.0, 0.0, 4.0]]])  # (n_trials, n_snr, 3)
    (row,) = rs.crb_rows("snr", GEO_A, "", (10.0,), bounds)
    assert row["method"] == "crb" and row["n"] == "2"
    assert float(row["rmse"]) == pytest.approx(np.sqrt(12.5), abs=1e-4)
    assert row["median"] == "nan"


def test_update_summary_replaces_only_its_stage_and_is_byte_stable(tmp_path):
    path = tmp_path / "results" / "summary.csv"
    est, truth = fake_estimates()
    snr_rows = rs.make_rows("snr", GEO_A, "", ("das",), (0.0, 10.0), est[:, :, :1], truth)
    scr_rows = rs.make_rows("screening", GEO_B, "", ("das",), (0.0,), est[:, :1, :1], truth)
    rs.update_summary(path, "snr", snr_rows)
    rs.update_summary(path, "screening", scr_rows)
    first = path.read_bytes()
    rs.update_summary(path, "snr", list(reversed(snr_rows)))
    assert path.read_bytes() == first
    rows = rs.read_summary(path)
    assert [r["stage"] for r in rows] == ["screening", "snr", "snr"]
    assert [r["snr_db"] for r in rows if r["stage"] == "snr"] == ["10", "0"]  # high SNR first
    rs.update_summary(path, "snr", [])
    assert {r["stage"] for r in rs.read_summary(path)} == {"screening"}
    with pytest.raises(ValueError, match="unknown stage"):
        rs.update_summary(path, "bogus", [])


def test_read_of_a_missing_file_is_empty(tmp_path):
    assert rs.read_summary(tmp_path / "nope.csv") == []


def rows_with_rmse(table):
    """``table = {geometry: {method: rmse}}`` as screening rows at 0 dB, plus a CRB row."""
    out = []
    for geo, methods in table.items():
        for method, value in methods.items():
            row = {c: "nan" for c in rs.COLUMNS}
            row.update(stage="screening", topology=geo.topology, variant="", method=method)
            row.update(
                diameter_mm=f"{geo.diameter * 1000:.0f}", height_mm=f"{geo.height * 1000:.0f}"
            )
            row.update(snr_db="0", rmse=f"{value:.4f}")
            out.append(row)
    return out


def test_geometry_score_is_the_geometric_mean_over_methods_without_crb():
    rows = rows_with_rmse({GEO_A: {"das": 1.0, "music": 4.0, "crb": 0.01}})
    assert rs.geometry_scores(rows, "screening")[GEO_A] == pytest.approx(2.0)
    assert rs.geometry_scores(rows, "confirm") == {}


def test_top_geometries_orders_by_score_then_size():
    scores = {GEO_A: 2.0, GEO_B: 1.0, GEO_C: 1.0}
    assert rs.top_geometries(scores, 2) == [GEO_B, GEO_C]  # tie: smaller diameter first
    assert rs.top_geometries(scores, 3)[-1] == GEO_A


def test_preferred_takes_the_smallest_within_the_margin():
    scores = {GEO_A: 1.10, GEO_B: 1.00, GEO_C: 1.14}
    assert rs.preferred(scores) == GEO_B  # smallest diameter among all three
    assert rs.preferred({GEO_A: 1.0, GEO_B: 1.2, GEO_C: 1.3}) == GEO_A  # B and C are not close
    assert rs.preferred({GEO_A: 1.0, GEO_B: 1.15}) == GEO_B  # exactly at the margin

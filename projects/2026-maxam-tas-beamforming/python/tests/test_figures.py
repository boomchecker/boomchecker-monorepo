"""``figures.py`` on synthetic ``summary.csv`` rows with a reduced beampattern table."""

import importlib.util
from pathlib import Path

import numpy as np
import pytest

from beamforming import beampattern as bp
from beamforming import experiment as ex
from beamforming import results as rs

SCRIPTS = Path(__file__).resolve().parents[1] / "scripts"


@pytest.fixture(scope="module")
def figures():
    spec = importlib.util.spec_from_file_location("figures", SCRIPTS / "figures.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def fake(stage, geo, snrs, methods, variant="", err=1.0, seed=0, n=12):
    rng = np.random.default_rng(seed)
    truth = ex.g.random_directions(n, rng)
    est = truth[:, None, None, :] + rng.normal(0, np.deg2rad(err), (n, len(snrs), len(methods), 3))
    est /= np.linalg.norm(est, axis=-1, keepdims=True)
    return rs.make_rows(stage, geo, variant, methods, snrs, est, truth)


@pytest.fixture
def summary(tmp_path):
    path = tmp_path / "summary.csv"
    ref = ex.Geometry("2x8_rot", 0.20, 0.07)
    geos = ex.sweep_geometries()
    rows = []
    for i, geo in enumerate(geos):
        rows += fake("screening", geo, (10.0, 0.0), ex.METHODS, err=1.0 + 0.1 * i, seed=i)
    rs.update_summary(path, "screening", rows)
    rs.update_summary(path, "confirm", fake("confirm", ref, (10.0, 0.0), ex.METHODS, err=2.0))
    snrs = tuple(float(s) for s in range(30, -15, -5))
    snr_rows = fake("snr", ref, snrs, ex.METHODS, err=1.5)
    bounds = np.abs(np.random.default_rng(1).normal(1.0, 0.1, (12, len(snrs), 3)))
    rs.update_summary(path, "snr", snr_rows + rs.crb_rows("snr", ref, "", snrs, bounds))
    ab = []
    for name, err in ((rs.ABLATION_BASELINE, 2.0), ("snr", 1.5), ("smooth1", 1.8)):
        methods = ("mvdr", "music") if name == "smooth1" else ("das", "mvdr", "srp_phat", "music")
        ab += fake("ablation", ref, (30.0, 0.0), methods, variant=name, err=err)
    rs.update_summary(path, "ablation", ab)
    return path


def test_main_writes_every_figure_and_table(figures, summary, tmp_path, monkeypatch):
    monkeypatch.setattr(ex, "sweep_geometries", lambda: list(figures.BEAM_CUT_GEOMETRY.values()))
    monkeypatch.setattr(figures, "TABLE_FREQS", (500.0, 1000.0))
    monkeypatch.setattr(
        figures, "beampattern_figure", lambda out, freq=1000.0: out.write_bytes(b"x")
    )
    monkeypatch.setattr(
        figures.bp, "evaluate", lambda mic, f, **k: bp.Result(40.0 + f / 100, 90.0, -8.4)
    )
    monkeypatch.setattr(
        "sys.argv",
        [
            "figures",
            "--summary",
            str(summary),
            "--figures",
            str(tmp_path / "fig"),
            "--generated",
            str(tmp_path / "gen"),
            "--beampattern-csv",
            str(tmp_path / "bp.csv"),
        ],
    )
    figures.main()
    assert (tmp_path / "fig" / "sweep.pdf").stat().st_size > 1000
    for name in ("ablation", "confirm", "beampattern", "cost"):
        assert (tmp_path / "gen" / f"{name}.tex").read_text().startswith("% generated")
    abl = (tmp_path / "gen" / "ablation.tex").read_text()
    assert "baseline & " in abl and "smooth1 & -- &" in abl  # no das/srp rows for smoothing
    assert "{,}" in (tmp_path / "gen" / "confirm.tex").read_text()


def test_sweep_pdf_has_no_timestamp(figures, summary, tmp_path):
    figures.style()
    rows = rs.read_summary(summary)
    a, b = tmp_path / "a.pdf", tmp_path / "b.pdf"
    figures.sweep_figure(rows, a)
    figures.sweep_figure(rows, b)
    assert a.read_bytes() == b.read_bytes()
    assert b"CreationDate" not in a.read_bytes()


def test_beampattern_figure_is_written(figures, tmp_path):
    figures.style()
    out = tmp_path / "bp.pdf"
    figures.beampattern_figure(out)
    assert out.stat().st_size > 1000


def test_missing_stage_is_a_clear_error(figures, tmp_path):
    with pytest.raises(SystemExit, match="snr"):
        figures.sweep_figure([], tmp_path / "x.pdf")


def test_width_cell_marks_an_omnidirectional_pattern(figures):
    assert figures.width_cell(360.0) == "--" and figures.width_cell(73.5) == "74"


def test_cost_macros_match_the_cost_module(figures, tmp_path):
    figures.cost_tex(tmp_path / "cost.tex")
    text = (tmp_path / "cost.tex").read_text()
    assert "\\newcommand{\\macMUSIC}" in text and "\\newcommand{\\macDirections}{1489}" in text

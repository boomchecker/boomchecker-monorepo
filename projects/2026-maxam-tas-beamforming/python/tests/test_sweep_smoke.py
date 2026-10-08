"""The sweep and ablation scripts end to end on a handful of clips, with a reduced setup."""

import argparse
import importlib.util
import json
from pathlib import Path

import pytest

from beamforming import experiment as ex
from beamforming import results as rs

SCRIPTS = Path(__file__).resolve().parents[1] / "scripts"
DATA = Path(__file__).resolve().parents[2] / "data" / "dads"


def load_script(name):
    spec = importlib.util.spec_from_file_location(name, SCRIPTS / f"{name}.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


@pytest.fixture
def sweep(monkeypatch):
    module = load_script("sweep")
    small = [
        ex.Geometry("1x8", 0.25),
        ex.Geometry("2x8_rot", 0.25, 0.10),
        ex.Geometry("2x8", 0.20, 0.07),
    ]
    monkeypatch.setattr(ex, "sweep_geometries", lambda: small)
    monkeypatch.setattr(module, "SNR_SWEEP", (20.0, 0.0))
    monkeypatch.setattr(module, "POSITION_SIGMAS_MM", (1.0,))
    return module


def args(tmp_path, **kw):
    sel = tmp_path / "selected.json"
    sel.write_text(json.dumps({"topology": "2x8_rot", "diameter_mm": 250, "height_mm": 100}))
    base = dict(
        data=DATA,
        summary=tmp_path / "summary.csv",
        selected=sel,
        comparisons=tmp_path / "comparisons.csv",
        raw=tmp_path / "out",
        dirs=1,
        limit=3,
        workers=1,
    )
    return argparse.Namespace(**{**base, **kw})


def test_stages_write_their_rows_and_the_summary_is_reproducible(sweep, tmp_path):
    a = args(tmp_path)
    sweep.stage_screening(a)
    sweep.stage_confirm(args(tmp_path, dirs=2))
    sweep.stage_snr(a)
    sweep.stage_sensitivity(a)
    rows = rs.read_summary(a.summary)
    stages = {r["stage"] for r in rows}
    assert stages == {"screening", "confirm", "snr", "sensitivity"}
    assert len(rs.stage_rows(rows, "screening")) == 3 * 2 * 5
    # top 3 of the 3 geometries, plus the two 250 mm ones again with the narrower band
    assert len(rs.stage_rows(rows, "confirm")) == 5 * 2 * 5
    assert {r["variant"] for r in rs.stage_rows(rows, "confirm")} == {"", "band1750"}
    assert {r["n"] for r in rs.stage_rows(rows, "confirm")} == {"6"}
    snr = rs.stage_rows(rows, "snr")
    assert {r["method"] for r in snr} == {*ex.METHODS, "crb"} and len(snr) == 2 * 6
    sens = rs.stage_rows(rows, "sensitivity")
    assert {r["variant"] for r in sens} == {"nominal", "c=331", "c=355", "pos=1mm"}
    assert {r["topology"] for r in sens} == {"2x8_rot", "1x8"}
    comps = rs.read_summary(a.comparisons)
    assert {r["stage"] for r in comps} == {"confirm", "snr", "sensitivity"}
    snr_comps = [r for r in comps if r["stage"] == "snr"]
    assert {r["b"] for r in snr_comps} == {"das"} and len(snr_comps) == 2 * 4
    assert all(float(r["low"]) <= float(r["ratio"]) <= float(r["high"]) for r in comps)
    narrow = [r for r in comps if r["a"].endswith("band1750") and r["b"] == "2x8_rot d250/h100"]
    assert narrow, "the narrow band is compared with the full band of the same array"
    for name in ("screening", "confirm", "snr", "sensitivity-2x8_rot", "sensitivity-1x8"):
        assert (tmp_path / "out" / f"{name}.npz").exists()
    first, first_comps = a.summary.read_bytes(), a.comparisons.read_bytes()
    sweep.stage_snr(a)
    assert a.summary.read_bytes() == first and a.comparisons.read_bytes() == first_comps


def test_confirm_needs_the_screening(sweep, tmp_path):
    with pytest.raises(SystemExit, match="screening"):
        sweep.stage_confirm(args(tmp_path))


def test_snr_and_sensitivity_need_the_selection(sweep, tmp_path):
    a = args(tmp_path, selected=tmp_path / "missing.json")
    with pytest.raises(FileNotFoundError):
        sweep.stage_snr(a)
    with pytest.raises(FileNotFoundError):
        sweep.stage_sensitivity(a)


def test_ablation_runs_every_variant_and_reports_the_rule(monkeypatch, tmp_path, capsys):
    ablate = load_script("ablate")
    keep = {k: ablate.VARIANTS[k] for k in (rs.ABLATION_BASELINE, "guard1", "smooth1")}
    monkeypatch.setattr(ablate, "VARIANTS", keep)
    monkeypatch.setattr(ablate, "SNRS", (30.0, 0.0))
    monkeypatch.setattr(
        "sys.argv",
        [
            "ablate",
            "--limit",
            "3",
            "--dirs",
            "1",
            "--workers",
            "1",
            "--data",
            str(DATA),
            "--summary",
            str(tmp_path / "s.csv"),
            "--comparisons",
            str(tmp_path / "c.csv"),
            "--raw",
            str(tmp_path / "out"),
        ],
    )
    ablate.main()
    rows = rs.read_summary(tmp_path / "s.csv")
    assert {r["variant"] for r in rows} == set(keep)
    assert len(rs.stage_rows(rows, "ablation", variant="smooth1")) == 2 * 2  # mvdr, music
    out = capsys.readouterr().out
    for name in ("guard1", "smooth1"):  # one verdict line per variant
        line = next(x for x in out.splitlines() if x.startswith(name) and "ratio" in x)
        assert line.rstrip().endswith(("ADOPT", "keep default"))
    comps = rs.read_summary(tmp_path / "c.csv")
    assert {r["a"] for r in comps} == {"guard1", "smooth1"}
    assert {r["method"] for r in comps if r["a"] == "smooth1"} == {"all", "mvdr", "music"}
    assert (tmp_path / "out" / "ablation-baseline.npz").exists()

"""Geometry sweep, SNR sweep with CRB and sensitivity runs (rows of ``summary.csv``).

Stages (each replaces its own rows in the summary):

* ``screening``: all 21 geometries, one direction per clip, SNR 10 and 0 dB, all methods,
* ``confirm``: the 3 best geometries of the screening (geometric mean of the angular RMSE over
  the methods at 0 dB), five directions per clip,
* ``snr``: the selected geometry (``python/results/selected.json``, written by hand after the
  confirm stage) at -10 to 30 dB in 5 dB steps, with the CRB,
* ``sensitivity``: DAS, SRP-PHAT, MVDR and MUSIC on the selected geometry and a 1x8 of the
  same diameter with a wrong speed of sound and microphone position errors at 10 dB.

Every stage is deterministic (see :mod:`beamforming.experiment`).
"""

from __future__ import annotations

import argparse
import os

# one BLAS thread per worker process: the work is parallel over trials, not inside a matrix
for _var in ("OMP_NUM_THREADS", "OPENBLAS_NUM_THREADS", "MKL_NUM_THREADS"):
    os.environ.setdefault(_var, "1")

from pathlib import Path  # noqa: E402

from beamforming import dads  # noqa: E402
from beamforming import experiment as ex  # noqa: E402
from beamforming import results as rs  # noqa: E402

GEOMETRY_SNRS = (10.0, 0.0)
SNR_SWEEP = tuple(float(s) for s in range(30, -15, -5))
SENSITIVITY_SNR = 10.0
SENSITIVITY_METHODS = ("das", "srp_phat", "mvdr", "music")
SOUND_SPEEDS = (331.0, 355.0)
POSITION_SIGMAS_MM = (0.5, 1.0, 2.0)


def run_geometries(
    stage: str, geometries: list[ex.Geometry], trials: list[ex.Trial], args: argparse.Namespace
) -> list[rs.Row]:
    conds = [ex.Condition(geo) for geo in geometries]
    evals = ex.evaluate(conds, trials, GEOMETRY_SNRS, ex.METHODS, workers=args.workers)
    truth = ex.truth(trials)
    rows: list[rs.Row] = []
    for geo, ev in zip(geometries, evals, strict=True):
        rows += rs.make_rows(stage, geo, "", ex.METHODS, GEOMETRY_SNRS, ev.est, truth)
    return rows


def print_ranking(rows: list[rs.Row], stage: str) -> dict[ex.Geometry, float]:
    scores = rs.geometry_scores(rows, stage, 0.0)
    print(f"\n{stage}: angular RMSE in degrees at 0 dB (score = geometric mean over methods)")
    print(f"{'geometry':18s} {'score':>6s} " + " ".join(f"{m:>11s}" for m in ex.METHODS))
    for geo in sorted(scores, key=lambda x: (scores[x], x.size_key())):
        by = {
            r["method"]: r["rmse"]
            for r in rs.stage_rows(
                rows,
                stage,
                topology=geo.topology,
                diameter_mm=f"{geo.diameter * 1000:.0f}",
                height_mm=f"{geo.height * 1000:.0f}",
                snr_db="0",
            )
        }
        print(
            f"{geo.label:18s} {scores[geo]:6.2f} " + " ".join(f"{by[m]:>11s}" for m in ex.METHODS)
        )
    return scores


def stage_screening(args: argparse.Namespace) -> None:
    trials = ex.load_trials(args.data, 1, args.limit)
    rows = run_geometries("screening", ex.sweep_geometries(), trials, args)
    rs.update_summary(args.summary, "screening", rows)
    scores = print_ranking(rows, "screening")
    print("\nconfirm candidates:", ", ".join(geo.label for geo in rs.top_geometries(scores)))


def stage_confirm(args: argparse.Namespace) -> None:
    screening = rs.read_summary(args.summary)
    scores = rs.geometry_scores(screening, "screening", 0.0)
    if not scores:
        raise SystemExit("no screening rows in the summary: run `sweep.py screening` first")
    top = rs.top_geometries(scores, 3)
    trials = ex.load_trials(args.data, args.dirs, args.limit)
    rows = run_geometries("confirm", top, trials, args)
    rs.update_summary(args.summary, "confirm", rows)
    confirmed = print_ranking(rows, "confirm")
    best = rs.preferred(confirmed)
    print(f"\nrule 'smallest within 15 % of the best': {best.label}")
    print("write the choice to python/results/selected.json (topology, diameter_mm, height_mm)")


def stage_snr(args: argparse.Namespace) -> None:
    selected = rs.load_selected(args.selected)
    geo: ex.Geometry = selected["geometry"]
    trials = ex.load_trials(args.data, args.dirs, args.limit)
    (ev,) = ex.evaluate([ex.Condition(geo)], trials, SNR_SWEEP, ex.METHODS, True, args.workers)
    assert ev.crb is not None
    rows = rs.make_rows("snr", geo, "", ex.METHODS, SNR_SWEEP, ev.est, ex.truth(trials))
    rows += rs.crb_rows("snr", geo, "", SNR_SWEEP, ev.crb)
    rs.update_summary(args.summary, "snr", rows)
    print(f"\n{geo.label}, {len(trials)} trials: angular RMSE (deg) vs SNR")
    print(f"{'SNR':>5s} " + " ".join(f"{m:>11s}" for m in (*ex.METHODS, "crb")))
    for snr in SNR_SWEEP:
        by = {r["method"]: r["rmse"] for r in rs.stage_rows(rows, "snr", snr_db=f"{snr:.0f}")}
        print(f"{snr:5.0f} " + " ".join(f"{by[m]:>11s}" for m in (*ex.METHODS, "crb")))


def stage_sensitivity(args: argparse.Namespace) -> None:
    selected = rs.load_selected(args.selected)
    geo: ex.Geometry = selected["geometry"]
    methods = tuple(selected.get("sensitivity_methods", SENSITIVITY_METHODS))
    contrast = ex.Geometry("1x8", geo.diameter)
    # name: (speed of sound in the simulation, std of the microphone position error in m)
    variants: dict[str, tuple[float, float]] = {"nominal": (ex.sg.C, 0.0)}
    variants |= {f"c={c:.0f}": (c, 0.0) for c in SOUND_SPEEDS}
    variants |= {f"pos={s:g}mm": (ex.sg.C, s / 1000) for s in POSITION_SIGMAS_MM}
    trials = ex.load_trials(args.data, args.dirs, args.limit)
    truth = ex.truth(trials)
    rows: list[rs.Row] = []
    geometries = [geo] if geo.topology == "1x8" else [geo, contrast]
    for g_ in geometries:
        conds = [ex.Condition(g_, c_true=c, sigma_pos=sig) for c, sig in variants.values()]
        evals = ex.evaluate(conds, trials, (SENSITIVITY_SNR,), methods, workers=args.workers)
        for name, ev in zip(variants, evals, strict=True):
            rows += rs.make_rows(
                "sensitivity", g_, name, methods, (SENSITIVITY_SNR,), ev.est, truth
            )
    rs.update_summary(args.summary, "sensitivity", rows)
    print(f"\n{len(trials)} trials at {SENSITIVITY_SNR:.0f} dB: angular RMSE (deg)")
    for g_ in geometries:
        print(f"\n{g_.label}\n{'variant':12s} " + " ".join(f"{m:>9s}" for m in methods))
        for name in variants:
            sel = rs.stage_rows(
                rows,
                "sensitivity",
                topology=g_.topology,
                diameter_mm=f"{g_.diameter * 1000:.0f}",
                height_mm=f"{g_.height * 1000:.0f}",
                variant=name,
            )
            by = {r["method"]: r["rmse"] for r in sel}
            print(f"{name:12s} " + " ".join(f"{by[m]:>9s}" for m in methods))


STAGES = {
    "screening": stage_screening,
    "confirm": stage_confirm,
    "snr": stage_snr,
    "sensitivity": stage_sensitivity,
}
DEFAULT_DIRS = {"screening": 1, "confirm": 5, "snr": 5, "sensitivity": 5}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("stage", choices=STAGES)
    parser.add_argument("--data", type=Path, default=dads.DEFAULT_OUT)
    parser.add_argument("--summary", type=Path, default=Path("python/results/summary.csv"))
    parser.add_argument("--selected", type=Path, default=Path("python/results/selected.json"))
    parser.add_argument("--dirs", type=int, default=0, help="directions per clip (stage default)")
    parser.add_argument("--limit", type=int, default=0, help="use only the first N clips (debug)")
    parser.add_argument("--workers", type=int, default=os.cpu_count() or 1)
    args = parser.parse_args()
    args.dirs = args.dirs or DEFAULT_DIRS[args.stage]
    STAGES[args.stage](args)
    print(f"wrote {args.summary}")


if __name__ == "__main__":
    main()

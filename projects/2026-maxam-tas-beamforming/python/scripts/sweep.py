"""Geometry sweep, SNR sweep with CRB and sensitivity runs (rows of ``summary.csv``).

Stages (each replaces its own rows in the summary):

* ``screening``: all 21 geometries, one direction per clip, SNR 10 and 0 dB, all methods,
* ``confirm``: the 3 best geometries of the screening (geometric mean of the angular RMSE over
  the methods at 0 dB), five directions per clip; a 250 mm candidate (ring spacing aliases at
  1.79 kHz) is also run with the band limited to 1750 Hz,
* ``snr``: the selected geometry (``python/results/selected.json``, written by hand after the
  confirm stage) at -10 to 30 dB in 5 dB steps, with the CRB,
* ``sensitivity``: DAS, SRP-PHAT, MVDR and MUSIC on the selected geometry and a 1x8 of the
  same diameter with a wrong speed of sound and microphone position errors at 10 dB.

Every stage is deterministic (see :mod:`beamforming.experiment`), stores its per-trial
estimates in ``python/out/<stage>.npz`` and writes paired RMSE ratios with bootstrap 95 %
intervals to ``python/results/comparisons.csv``.
"""

from __future__ import annotations

import argparse
import os

# one BLAS thread per worker process: the work is parallel over trials, not inside a matrix
for _var in ("OMP_NUM_THREADS", "OPENBLAS_NUM_THREADS", "MKL_NUM_THREADS"):
    os.environ.setdefault(_var, "1")

from dataclasses import replace  # noqa: E402
from pathlib import Path  # noqa: E402

from beamforming import dads, doa  # noqa: E402
from beamforming import experiment as ex  # noqa: E402
from beamforming import metrics as mt  # noqa: E402
from beamforming import results as rs  # noqa: E402

GEOMETRY_SNRS = (10.0, 0.0)
SNR_SWEEP = tuple(float(s) for s in range(30, -15, -5))
SENSITIVITY_SNR = 10.0
SENSITIVITY_METHODS = ("das", "srp_phat", "mvdr", "music")
SOUND_SPEEDS = (331.0, 355.0)
POSITION_SIGMAS_MM = (0.5, 1.0, 2.0)
# arrays whose ring spacing aliases below the band edge are also run with a narrower band
ALIAS_BAND = (300.0, 1750.0)
ALIAS_VARIANT = "band1750"
ALIAS_DIAMETER = 0.25


def geometry_comparisons(
    stage: str,
    labels: list[str],
    errors: list,
    reference: int,
    snrs: tuple[float, ...],
    n: int,
) -> list[rs.Row]:
    """Each condition against ``labels[reference]``: over the methods and per method."""
    out = []
    for k, label in enumerate(labels):
        if k == reference:
            continue
        for s, snr in enumerate(snrs):
            ci = mt.paired_ratio(errors[k][:, s, :], errors[reference][:, s, :])
            out.append(rs.comparison_row(stage, label, labels[reference], "all", snr, n, ci))
            for m, method in enumerate(ex.METHODS):
                ci = mt.paired_ratio(errors[k][:, s, m], errors[reference][:, s, m])
                out.append(rs.comparison_row(stage, label, labels[reference], method, snr, n, ci))
    return out


def run_geometries(
    stage: str,
    geometries: list[ex.Geometry],
    trials: list[ex.Trial],
    args: argparse.Namespace,
    variants: list[str] | None = None,
) -> tuple[list[rs.Row], list[str], list]:
    """Rows, labels and per-trial errors of every geometry (``variants`` set the band)."""
    variants = variants or [""] * len(geometries)
    conds = [
        ex.Condition(
            geo, replace(doa.DEFAULT, band=ALIAS_BAND) if v == ALIAS_VARIANT else doa.DEFAULT
        )
        for geo, v in zip(geometries, variants, strict=True)
    ]
    evals = ex.evaluate(conds, trials, GEOMETRY_SNRS, ex.METHODS, workers=args.workers)
    truth = ex.truth(trials)
    rows: list[rs.Row] = []
    labels = [f"{geo.label} {v}".strip() for geo, v in zip(geometries, variants, strict=True)]
    for geo, v, ev in zip(geometries, variants, evals, strict=True):
        rows += rs.make_rows(stage, geo, v, ex.METHODS, GEOMETRY_SNRS, ev.est, truth)
    ex.save_raw(
        args.raw / f"{stage}.npz", trials, labels, evals, snrs=GEOMETRY_SNRS, methods=ex.METHODS
    )
    return rows, labels, [ex.angular_errors(ev.est, trials) for ev in evals]


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
                variant="",
                snr_db="0",
            )
        }
        print(
            f"{geo.label:18s} {scores[geo]:6.2f} " + " ".join(f"{by[m]:>11s}" for m in ex.METHODS)
        )
    return scores


def stage_screening(args: argparse.Namespace) -> None:
    trials = ex.load_trials(args.data, 1, args.limit)
    rows, _, _ = run_geometries("screening", ex.sweep_geometries(), trials, args)
    rs.update_summary(args.summary, "screening", rows)
    scores = print_ranking(rows, "screening")
    print("\nconfirm candidates:", ", ".join(geo.label for geo in rs.top_geometries(scores)))


def stage_confirm(args: argparse.Namespace) -> None:
    screening = rs.read_summary(args.summary)
    scores = rs.geometry_scores(screening, "screening", 0.0)
    if not scores:
        raise SystemExit("no screening rows in the summary: run `sweep.py screening` first")
    top = rs.top_geometries(scores, 3)
    aliased = [geo for geo in top if geo.diameter >= ALIAS_DIAMETER]
    geometries = top + aliased
    variants = [""] * len(top) + [ALIAS_VARIANT] * len(aliased)
    trials = ex.load_trials(args.data, args.dirs, args.limit)
    rows, labels, errors = run_geometries("confirm", geometries, trials, args, variants)
    rs.update_summary(args.summary, "confirm", rows)
    confirmed = print_ranking(rows, "confirm")
    best = min(range(len(top)), key=lambda k: confirmed[top[k]])
    comps = geometry_comparisons("confirm", labels, errors, best, GEOMETRY_SNRS, len(trials))
    for k, geo in enumerate(aliased):  # the narrower band against the full band, same array
        full, narrow = top.index(geo), len(top) + k
        comps += geometry_comparisons(
            "confirm",
            [labels[narrow], labels[full]],
            [errors[narrow], errors[full]],
            1,
            GEOMETRY_SNRS,
            len(trials),
        )
    rs.update_comparisons(args.comparisons, "confirm", comps)
    if aliased:
        narrow_scores = rs.geometry_scores(rows, "confirm", 0.0, ALIAS_VARIANT)
        print(f"\nscore with the band {ALIAS_BAND[0]:.0f} to {ALIAS_BAND[1]:.0f} Hz:")
        for geo, score in narrow_scores.items():
            print(f"  {geo.label:18s} {score:6.2f}")
    print("\npaired RMSE ratios against the best geometry (0 dB, over the methods, 95 % CI):")
    for r in comps:
        if r["method"] == "all" and r["snr_db"] == "0":
            print(f"  {r['a']:28s} / {r['b']:18s} {r['ratio']} [{r['low']}, {r['high']}]")
    best_ = rs.preferred(confirmed)
    print(f"\nrule 'smallest within 15 % of the best': {best_.label}")
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
    ex.save_raw(
        args.raw / "snr.npz",
        trials,
        [geo.label],
        [ev],
        snrs=SNR_SWEEP,
        methods=ex.METHODS,
        crb=ev.crb,
    )
    err = ex.angular_errors(ev.est, trials)
    das = ex.METHODS.index("das")
    comps = [
        rs.comparison_row(
            "snr", m, "das", m, snr, len(trials), mt.paired_ratio(err[:, s, k], err[:, s, das])
        )
        for s, snr in enumerate(SNR_SWEEP)
        for k, m in enumerate(ex.METHODS)
        if m != "das"
    ]
    rs.update_comparisons(args.comparisons, "snr", comps)
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
    comps: list[rs.Row] = []
    for g_ in geometries:
        conds = [ex.Condition(g_, c_true=c, sigma_pos=sig) for c, sig in variants.values()]
        evals = ex.evaluate(conds, trials, (SENSITIVITY_SNR,), methods, workers=args.workers)
        for name, ev in zip(variants, evals, strict=True):
            rows += rs.make_rows(
                "sensitivity", g_, name, methods, (SENSITIVITY_SNR,), ev.est, truth
            )
        labels = [f"{g_.label} {name}" for name in variants]
        ex.save_raw(
            args.raw / f"sensitivity-{g_.topology}.npz", trials, labels, evals, methods=methods
        )
        err = [ex.angular_errors(ev.est, trials) for ev in evals]
        for k, label in enumerate(labels[1:], start=1):
            for m, method in enumerate(methods):
                ci = mt.paired_ratio(err[k][:, 0, m], err[0][:, 0, m])
                comps.append(
                    rs.comparison_row(
                        "sensitivity", label, labels[0], method, SENSITIVITY_SNR, len(trials), ci
                    )
                )
    rs.update_summary(args.summary, "sensitivity", rows)
    rs.update_comparisons(args.comparisons, "sensitivity", comps)
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
    parser.add_argument("--comparisons", type=Path, default=Path("python/results/comparisons.csv"))
    parser.add_argument("--raw", type=Path, default=Path("python/out"), help="per-trial npz dir")
    parser.add_argument("--dirs", type=int, default=0, help="directions per clip (stage default)")
    parser.add_argument("--limit", type=int, default=0, help="use only the first N clips (debug)")
    parser.add_argument("--workers", type=int, default=os.cpu_count() or 1)
    args = parser.parse_args()
    args.dirs = args.dirs or DEFAULT_DIRS[args.stage]
    STAGES[args.stage](args)
    print(f"wrote {args.summary}")


if __name__ == "__main__":
    main()

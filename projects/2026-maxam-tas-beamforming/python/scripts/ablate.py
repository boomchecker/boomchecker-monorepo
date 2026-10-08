"""Ablation of the optional processing choices on the reference geometry.

Each variant changes one option of ``doa.Config`` (band guard, SNR bin gain, frequency
smoothing of the covariance, 75 % STFT overlap) against the processing of M3, on the same
paired trials at 30, 10 and 0 dB. The rule fixed for M4 decides whether an option becomes the
default: it must lower the RMSE at 0 dB by at least 5 % (geometric mean over the methods it
affects) and not raise it at 30 dB by more than 5 %. The script only reports the decision; the
defaults in ``doa.Config`` are changed by hand (the SNR gain was adopted). Rows go to
``summary.csv`` (stage ``ablation``).
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
from beamforming import results as rs  # noqa: E402

REFERENCE = ex.Geometry("2x8_rot", 0.20, 0.07)
SNRS = (30.0, 10.0, 0.0)
GRID = ("das", "mvdr", "srp_phat", "music")
COVARIANCE = ("mvdr", "music")  # the covariance is the only place where smoothing acts

# the processing of M3 (``bin_weighting="max"``) is pinned, so the baseline stays the same after
# the SNR gain became the default; variants change one option of it
M3 = {"bin_weighting": "max"}
# name: (Config overrides, methods that the option can change)
VARIANTS: dict[str, tuple[dict, tuple[str, ...]]] = {
    rs.ABLATION_BASELINE: (M3, GRID),
    "guard1": (M3 | {"guard_bins": 1}, GRID),
    "guard2": (M3 | {"guard_bins": 2}, GRID),
    "snr": ({"bin_weighting": "snr"}, GRID),
    "smooth1": (M3 | {"freq_smooth": 1}, COVARIANCE),
    "smooth2": (M3 | {"freq_smooth": 2}, COVARIANCE),
    "hop128": (M3 | {"hop": 128}, GRID),
    "snr+guard1": ({"bin_weighting": "snr", "guard_bins": 1}, GRID),
}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data", type=Path, default=dads.DEFAULT_OUT)
    parser.add_argument("--summary", type=Path, default=Path("python/results/summary.csv"))
    parser.add_argument("--dirs", type=int, default=5, help="directions per clip")
    parser.add_argument("--limit", type=int, default=0, help="use only the first N clips (debug)")
    parser.add_argument("--workers", type=int, default=os.cpu_count() or 1)
    args = parser.parse_args()

    trials = ex.load_trials(args.data, args.dirs, args.limit)
    truth = ex.truth(trials)
    rows: list[rs.Row] = []
    for name, (overrides, methods) in VARIANTS.items():
        cond = ex.Condition(REFERENCE, replace(doa.DEFAULT, **overrides))
        (ev,) = ex.evaluate([cond], trials, SNRS, methods, workers=args.workers)
        rows += rs.make_rows("ablation", REFERENCE, name, methods, SNRS, ev.est, truth)
        print(f"{name}: done", flush=True)
    rs.update_summary(args.summary, "ablation", rows)

    print(f"\n{len(trials)} trials on {REFERENCE.label}; angular RMSE in degrees")
    header = " ".join(f"{m:>9s}" for m in GRID)
    for snr in SNRS:
        print(f"\nSNR {snr:.0f} dB\n{'variant':12s} {header}")
        for name in VARIANTS:
            by = {
                r["method"]: r["rmse"]
                for r in rs.stage_rows(rows, "ablation", variant=name, snr_db=f"{snr:.0f}")
            }
            print(f"{name:12s} " + " ".join(f"{by.get(m, '-'):>9s}" for m in GRID))
    print("\nrule: ratio at 0 dB <= 0.95 and ratio at 30 dB <= 1.05 (geometric mean over methods)")
    for name in VARIANTS:
        if name == rs.ABLATION_BASELINE:
            continue
        r0, r30 = rs.variant_ratio(rows, name, 0.0), rs.variant_ratio(rows, name, 30.0)
        verdict = "ADOPT" if rs.adopt(rows, name) else "keep default"
        print(f"{name:12s} ratio 0 dB {r0:5.3f}   30 dB {r30:5.3f}   {verdict}")
    print(f"wrote {args.summary}")


if __name__ == "__main__":
    main()

"""``summary.csv``: one row per stage, condition, method and SNR, and the ranking built on it.

Stages write their own rows and leave the rest of the file alone, so the file can be rebuilt
piece by piece and stays byte-identical when nothing changed (fixed row order, fixed number
format). ``method == "crb"`` rows hold the Cramér-Rao bound in the ``rmse*`` columns.
"""

from __future__ import annotations

import csv
import math
from collections.abc import Iterable, Sequence
from pathlib import Path

import numpy as np
from numpy.typing import NDArray

from . import metrics as mt
from .experiment import Geometry

STAGES = ("ablation", "screening", "confirm", "snr", "sensitivity")
KEY = ("topology", "diameter_mm", "height_mm", "variant", "method", "snr_db")
STATS = (
    "n",
    "rmse",
    "median",
    "rmse_az",
    "rmse_el",
    "outliers",
    *(f"rmse_band{b}" for b in range(mt.N_BANDS)),
    *(f"outliers_band{b}" for b in range(mt.N_BANDS)),
)
COLUMNS = ("stage", *KEY, *STATS)
Row = dict[str, str]


def _fmt(v: float) -> str:
    return "nan" if math.isnan(v) else "inf" if math.isinf(v) else f"{v:.4f}"


def make_rows(
    stage: str,
    geometry: Geometry,
    variant: str,
    methods: Sequence[str],
    snrs: Sequence[float],
    est: NDArray,
    truth: NDArray,
) -> list[Row]:
    """Rows for ``est`` (n_trials, n_snr, n_methods, 3) against ``truth`` (n_trials, 3)."""
    rows = []
    for s, snr in enumerate(snrs):
        for m, method in enumerate(methods):
            stats = mt.summarize(mt.errors(est[:, s, m], truth)).row()
            rows.append(
                {
                    "stage": stage,
                    "topology": geometry.topology,
                    "diameter_mm": f"{geometry.diameter * 1000:.0f}",
                    "height_mm": f"{geometry.height * 1000:.0f}",
                    "variant": variant,
                    "method": method,
                    "snr_db": f"{snr:.0f}",
                    **{k: _fmt(stats[k]) for k in STATS},
                }
            )
    return rows


def crb_rows(
    stage: str, geometry: Geometry, variant: str, snrs: Sequence[float], bounds: NDArray
) -> list[Row]:
    """CRB rows from per-trial bounds ``(n_trials, n_snr, 3)`` in degrees (RMS over trials)."""
    rms = np.sqrt(np.mean(np.asarray(bounds) ** 2, axis=0))  # (n_snr, 3): az, el, angular
    return [
        {
            "stage": stage,
            "topology": geometry.topology,
            "diameter_mm": f"{geometry.diameter * 1000:.0f}",
            "height_mm": f"{geometry.height * 1000:.0f}",
            "variant": variant,
            "method": "crb",
            "snr_db": f"{snr:.0f}",
            **{k: "nan" for k in STATS},
            "n": str(len(bounds)),
            "rmse": _fmt(float(rms[s, 2])),
            "rmse_az": _fmt(float(rms[s, 0])),
            "rmse_el": _fmt(float(rms[s, 1])),
        }
        for s, snr in enumerate(snrs)
    ]


def _sort_key(row: Row) -> tuple:
    return (
        STAGES.index(row["stage"]),
        row["topology"],
        float(row["diameter_mm"]),
        float(row["height_mm"]),
        row["variant"],
        row["method"],
        -float(row["snr_db"]),
    )


def read_summary(path: Path) -> list[Row]:
    if not Path(path).exists():
        return []
    with open(path, newline="") as f:
        return list(csv.DictReader(f))


def update_summary(path: Path, stage: str, rows: Iterable[Row]) -> None:
    """Replace all rows of ``stage`` in the file (created when missing) and keep the others."""
    if stage not in STAGES:
        raise ValueError(f"unknown stage {stage!r}, expected one of {STAGES}")
    kept = [r for r in read_summary(path) if r["stage"] != stage]
    merged = sorted([*kept, *rows], key=_sort_key)
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=COLUMNS, lineterminator="\n")
        writer.writeheader()
        writer.writerows(merged)


def stage_rows(rows: Iterable[Row], stage: str, **match: str) -> list[Row]:
    """Rows of a stage whose columns equal ``match`` (values as strings)."""
    return [r for r in rows if r["stage"] == stage and all(r[k] == v for k, v in match.items())]


def geometry_of(row: Row) -> Geometry:
    height = float(row["height_mm"]) / 1000
    return Geometry(row["topology"], float(row["diameter_mm"]) / 1000, height)  # type: ignore[arg-type]


def geometry_scores(rows: Iterable[Row], stage: str, snr_db: float = 0.0) -> dict[Geometry, float]:
    """Geometric mean over the methods of the angular RMSE at ``snr_db``, per geometry.

    Methods are weighed equally so that the geometry is ranked, not the method that happens to
    be best on it; the CRB rows are not a method.
    """
    logs: dict[Geometry, list[float]] = {}
    for r in stage_rows(rows, stage, snr_db=f"{snr_db:.0f}", variant=""):
        if r["method"] != "crb":
            logs.setdefault(geometry_of(r), []).append(math.log(float(r["rmse"])))
    return {geo: math.exp(sum(v) / len(v)) for geo, v in logs.items()}


def top_geometries(scores: dict[Geometry, float], k: int = 3) -> list[Geometry]:
    """The ``k`` best geometries by score (ties broken by the smaller size)."""
    return sorted(scores, key=lambda geo: (scores[geo], geo.size_key()))[:k]


def preferred(scores: dict[Geometry, float], margin: float = mt.INDISTINGUISHABLE) -> Geometry:
    """Smallest geometry whose score is within ``margin`` of the best one.

    Smaller means a smaller diameter, then a smaller height; ``1x8`` (height 0) is smallest.
    """
    best = min(scores.values())
    near = [geo for geo, s in scores.items() if s <= best * (1 + margin)]
    return min(near, key=lambda geo: (*geo.size_key(), scores[geo]))

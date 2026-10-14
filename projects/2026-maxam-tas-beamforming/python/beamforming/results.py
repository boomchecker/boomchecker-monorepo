"""``summary.csv``: one row per stage, condition, method and SNR, and the ranking built on it.

Stages write their own rows and leave the rest of the file alone, so the file can be rebuilt
piece by piece and stays byte-identical when nothing changed (fixed row order, fixed number
format). ``method == "crb"`` rows hold the Cramér-Rao bound in the ``rmse*`` columns.
"""

from __future__ import annotations

import csv
import json
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
                    **{k: str(len(truth)) if k == "n" else _fmt(stats[k]) for k in STATS},
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


def _replace_stage(path: Path, stage: str, rows: Iterable[Row], columns: tuple, key) -> None:
    if stage not in STAGES:
        raise ValueError(f"unknown stage {stage!r}, expected one of {STAGES}")
    kept = [r for r in read_summary(path) if r["stage"] != stage]
    merged = sorted([*kept, *rows], key=key)
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=columns, lineterminator="\n")
        writer.writeheader()
        writer.writerows(merged)


def update_summary(path: Path, stage: str, rows: Iterable[Row]) -> None:
    """Replace all rows of ``stage`` in the file (created when missing) and keep the others."""
    _replace_stage(path, stage, rows, COLUMNS, _sort_key)


COMPARISON_COLUMNS = ("stage", "a", "b", "method", "snr_db", "n", "ratio", "low", "high")


def comparison_row(
    stage: str, a: str, b: str, method: str, snr_db: float, n: int, ci: tuple[float, float, float]
) -> Row:
    """One paired comparison: RMSE of ``a`` over RMSE of ``b`` with its 95 % interval."""
    ratio, low, high = ci
    return {
        "stage": stage,
        "a": a,
        "b": b,
        "method": method,
        "snr_db": f"{snr_db:.0f}",
        "n": str(n),
        "ratio": f"{ratio:.4f}",
        "low": f"{low:.4f}",
        "high": f"{high:.4f}",
    }


def _comparison_key(row: Row) -> tuple:
    return (STAGES.index(row["stage"]), row["a"], row["b"], row["method"], -float(row["snr_db"]))


def update_comparisons(path: Path, stage: str, rows: Iterable[Row]) -> None:
    """Replace the comparison rows of ``stage`` in ``comparisons.csv``."""
    _replace_stage(path, stage, rows, COMPARISON_COLUMNS, _comparison_key)


def stage_rows(rows: Iterable[Row], stage: str, **match: str) -> list[Row]:
    """Rows of a stage whose columns equal ``match`` (values as strings)."""
    return [r for r in rows if r["stage"] == stage and all(r[k] == v for k, v in match.items())]


def geometry_of(row: Row) -> Geometry:
    height = float(row["height_mm"]) / 1000
    return Geometry(row["topology"], float(row["diameter_mm"]) / 1000, height)  # type: ignore[arg-type]


def geometry_scores(
    rows: Iterable[Row], stage: str, snr_db: float = 0.0, variant: str = ""
) -> dict[Geometry, float]:
    """Geometric mean over the methods of the angular RMSE at ``snr_db``, per geometry.

    Methods are weighed equally so that the geometry is ranked, not the method that happens to
    be best on it; the CRB rows are not a method.
    """
    logs: dict[Geometry, list[float]] = {}
    for r in stage_rows(rows, stage, snr_db=f"{snr_db:.0f}", variant=variant):
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


ADOPT_GAIN = 0.95  # an option is adopted if it lowers the RMSE at 0 dB by at least 5 %
ADOPT_LOSS = 1.05  # and does not raise the RMSE at 30 dB by more than 5 %
ABLATION_BASELINE = "baseline"


def variant_ratio(rows: Iterable[Row], variant: str, snr_db: float) -> float:
    """Geometric mean over the methods of ``variant`` of its RMSE relative to the baseline."""
    rows = list(rows)
    base = {
        r["method"]: float(r["rmse"])
        for r in stage_rows(rows, "ablation", variant=ABLATION_BASELINE, snr_db=f"{snr_db:.0f}")
    }
    logs = [
        math.log(float(r["rmse"]) / base[r["method"]])
        for r in stage_rows(rows, "ablation", variant=variant, snr_db=f"{snr_db:.0f}")
        if r["method"] in base
    ]
    if not logs:
        raise ValueError(f"no ablation rows for {variant!r} at {snr_db:.0f} dB")
    return math.exp(sum(logs) / len(logs))


def adopt(rows: Iterable[Row], variant: str, low_snr: float = 0.0, high_snr: float = 30.0) -> bool:
    """Whether an ablation variant meets the rule for becoming the default."""
    rows = list(rows)
    return (
        variant_ratio(rows, variant, low_snr) <= ADOPT_GAIN
        and variant_ratio(rows, variant, high_snr) <= ADOPT_LOSS
    )


def load_selected(path: Path) -> dict:
    """The geometry chosen after the sweep (``selected.json``) as a dict with a ``geometry``."""
    path = Path(path)
    if not path.exists():
        raise FileNotFoundError(
            f"{path} missing: pick the geometry from the confirm stage first and write it there"
        )
    raw = json.loads(path.read_text())
    out = dict(raw)
    out["geometry"] = Geometry(raw["topology"], raw["diameter_mm"] / 1000, raw["height_mm"] / 1000)
    return out

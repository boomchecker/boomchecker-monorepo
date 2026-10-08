"""Error metrics of the geometry sweep.

A trial is one estimated direction against the true one, both unit vectors ``(n, 3)``.
Errors are reported in degrees:

* ``angular``: great-circle angle (what a pointing error costs),
* ``azimuth``: wrapped azimuth difference times ``cos(el_true)``, so it is the arc length on
  the sphere; trials with ``el_true`` above ``ZENITH_DEG`` are left out (azimuth is undefined),
* ``elevation``: absolute elevation difference.

Trials are also split into three elevation bands of equal solid angle (``sin(el)`` in thirds),
because planar arrays are weak near the horizon and a uniform hemisphere has as many trials
per solid angle there as at the zenith.
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np
from numpy.typing import NDArray

from . import geometry as g

ZENITH_DEG = 85.0
OUTLIER_DEG = 5.0
INDISTINGUISHABLE = 0.15  # relative RMSE differences below this are not resolved by 100 trials
BAND_EDGES_DEG = tuple(float(np.rad2deg(np.arcsin(s))) for s in (0.0, 1 / 3, 2 / 3, 1.0))
N_BANDS = len(BAND_EDGES_DEG) - 1


def elevation_band(el_true_deg: NDArray | float) -> NDArray[np.int64]:
    """Index 0..2 of the equal-area elevation band (0 to 19.5, 19.5 to 41.8, 41.8 to 90 deg)."""
    el = np.asarray(el_true_deg, dtype=float)
    return np.clip(np.searchsorted(BAND_EDGES_DEG, el, side="right") - 1, 0, N_BANDS - 1)


@dataclass(frozen=True)
class Errors:
    """Per-trial errors in degrees; ``azimuth`` is NaN where the azimuth is undefined."""

    angular: NDArray[np.float64]
    azimuth: NDArray[np.float64]
    elevation: NDArray[np.float64]
    band: NDArray[np.int64]


def errors(u_est: NDArray, u_true: NDArray) -> Errors:
    """Per-trial errors of estimated against true directions, both ``(n, 3)``."""
    u_est = np.atleast_2d(np.asarray(u_est, dtype=float))
    u_true = np.atleast_2d(np.asarray(u_true, dtype=float))
    az_e, el_e = g.to_angles(u_est)
    az_t, el_t = g.to_angles(u_true)
    el_true_deg = np.rad2deg(el_t)
    azimuth = g.azimuth_error_deg(az_e, az_t) * np.cos(el_t)
    azimuth = np.where(el_true_deg > ZENITH_DEG, np.nan, azimuth)
    return Errors(
        angular=g.angular_error_deg(u_est, u_true),
        azimuth=azimuth,
        elevation=np.abs(np.rad2deg(el_e) - el_true_deg),
        band=elevation_band(el_true_deg),
    )


def rmse(e: NDArray) -> float:
    """Root mean square over the finite entries, NaN for none."""
    e = np.asarray(e, dtype=float)
    e = e[np.isfinite(e)]
    return float(np.sqrt(np.mean(e**2))) if e.size else float("nan")


def outlier_fraction(angular: NDArray, threshold_deg: float = OUTLIER_DEG) -> float:
    """Share of trials with an angular error above ``threshold_deg``, NaN for no trial."""
    angular = np.asarray(angular, dtype=float)
    return float(np.mean(angular > threshold_deg)) if angular.size else float("nan")


@dataclass(frozen=True)
class Summary:
    """Aggregate of one configuration, method and SNR (columns of ``summary.csv``)."""

    n: int
    rmse: float
    median: float
    rmse_az: float
    rmse_el: float
    outliers: float
    band_rmse: tuple[float, ...]
    band_outliers: tuple[float, ...]

    def row(self) -> dict[str, float]:
        out = {
            "n": float(self.n),
            "rmse": self.rmse,
            "median": self.median,
            "rmse_az": self.rmse_az,
            "rmse_el": self.rmse_el,
            "outliers": self.outliers,
        }
        for b in range(N_BANDS):
            out[f"rmse_band{b}"] = self.band_rmse[b]
            out[f"outliers_band{b}"] = self.band_outliers[b]
        return out


def summarize(e: Errors) -> Summary:
    """Aggregate per-trial errors; an empty elevation band gives NaN."""
    bands = [e.band == b for b in range(N_BANDS)]
    return Summary(
        n=len(e.angular),
        rmse=rmse(e.angular),
        median=float(np.median(e.angular)) if len(e.angular) else float("nan"),
        rmse_az=rmse(e.azimuth),
        rmse_el=rmse(e.elevation),
        outliers=outlier_fraction(e.angular),
        band_rmse=tuple(rmse(e.angular[m]) for m in bands),
        band_outliers=tuple(outlier_fraction(e.angular[m]) for m in bands),
    )


def distinguishable(a: float, b: float, margin: float = INDISTINGUISHABLE) -> bool:
    """Whether two RMSEs differ by more than ``margin`` relative to the smaller one."""
    return abs(a - b) > margin * min(a, b)

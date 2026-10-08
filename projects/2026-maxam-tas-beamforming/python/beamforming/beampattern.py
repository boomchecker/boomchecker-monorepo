"""Delay-and-sum beampattern of a microphone array (uniform weights, far field).

``B(u) = |a(u0)^H a(u)|^2 / M^2`` is 1 at the steering direction ``u0``. It depends only on
the geometry, not on the signal, which makes it the fair way to compare arrays; MVDR and
MUSIC maps depend on the data. The main lobe width is measured on two great circles through
``u0`` (along the east and north tangent directions, see :func:`geometry.tangent_basis`) as
the angle between the two -3 dB points, so it is comparable to an angular error. The peak
sidelobe level is the highest local maximum of ``B`` on the upper hemisphere other than the
main lobe; it is NaN when the pattern has no sidelobe, and close to 0 dB when a grating
lobe from spatial aliasing appears.
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np
from numpy.typing import NDArray

from . import geometry as g
from . import signals as sg

FREQS_HZ = (300.0, 500.0, 1000.0, 2000.0, 3000.0)
STEER_EL_DEG = 45.0
HALF_POWER = 0.5
CUT_STEP_DEG = 0.1
GRID_STEP_DEG = 1.0
AZIMUTHS_DEG = tuple(float(a) for a in range(0, 360, 15))


def power(mic_pos: NDArray, freq: float, u0: NDArray, dirs: NDArray, c: float = sg.C) -> NDArray:
    """Normalised beampattern ``B`` (D,) for steering direction ``u0`` over ``dirs`` (D, 3)."""
    return _power(sg.steering(mic_pos, np.array([freq]), dirs, c)[:, 0, :], mic_pos, freq, u0, c)


def _power(A: NDArray, mic_pos: NDArray, freq: float, u0: NDArray, c: float) -> NDArray:
    """``B`` from precomputed steering vectors ``A`` (M, D) of the evaluation directions."""
    a0 = sg.steering(mic_pos, np.array([freq]), np.asarray(u0, dtype=float)[None, :], c)[:, 0, 0]
    return np.abs(a0.conj() @ A) ** 2 / len(mic_pos) ** 2


def lobe_width_deg(mic_pos: NDArray, freq: float, u0: NDArray, axis: str, c: float = sg.C) -> float:
    """Angle between the -3 dB points around ``u0`` on the great circle along ``axis``.

    ``axis`` is ``"east"`` (the azimuth direction) or ``"north"`` (the elevation direction).
    A side that stays above -3 dB over the whole half circle counts as 180 deg.
    """
    east, north = g.tangent_basis(u0)
    t = {"east": east, "north": north}[axis]
    phi = np.deg2rad(np.arange(0.0, 180.0 + CUT_STEP_DEG / 2, CUT_STEP_DEG))
    u0 = np.asarray(u0, dtype=float)
    total = 0.0
    for sign in (1.0, -1.0):
        dirs = np.cos(phi)[:, None] * u0[None, :] + sign * np.sin(phi)[:, None] * t[None, :]
        b = power(mic_pos, freq, u0, dirs, c)
        below = b < HALF_POWER
        k = int(np.argmax(below)) if below.any() else len(phi) - 1
        if below.any() and k > 0:  # linear interpolation between the samples around the crossing
            frac = (b[k - 1] - HALF_POWER) / (b[k - 1] - b[k])
            total += float(np.rad2deg(phi[k - 1] + frac * (phi[k] - phi[k - 1])))
        else:
            total += float(np.rad2deg(phi[k]))
    return total


def hemisphere_grid(step_deg: float = GRID_STEP_DEG) -> tuple[NDArray, tuple[int, int]]:
    """Unit vectors (n_az * n_el, 3) of the upper hemisphere, zenith row left out."""
    n_az = int(round(360.0 / step_deg))
    n_el = int(round(90.0 / step_deg))  # elevations 0 .. 90 - step
    az, el = np.meshgrid(
        np.deg2rad(step_deg * np.arange(n_az)),
        np.deg2rad(step_deg * np.arange(n_el)),
        indexing="ij",
    )
    return g.unit_vector(az.ravel(), el.ravel()), (n_az, n_el)


def peak_sidelobe_db(b_map: NDArray, main: tuple[int, int]) -> float:
    """Highest local maximum of ``b_map`` (n_az, n_el) other than the main lobe, in dB.

    Azimuth wraps around; elevation edges have fewer neighbours. ``main`` is the grid index of
    the steering direction; the main lobe is the local maximum reachable from it by ascent.
    """
    n_az, n_el = b_map.shape
    pad = np.pad(b_map, ((1, 1), (1, 1)), mode="constant", constant_values=-np.inf)
    pad[0, 1:-1], pad[-1, 1:-1] = b_map[-1], b_map[0]  # azimuth wrap
    is_max = np.ones_like(b_map, dtype=bool)
    for da in (-1, 0, 1):
        for de in (-1, 0, 1):
            if da or de:
                is_max &= b_map >= pad[1 + da : 1 + da + n_az, 1 + de : 1 + de + n_el]
    peaks = np.argwhere(is_max)
    best = -np.inf
    i, j = main
    for a, e in peaks:
        if _same_lobe(b_map, (int(a), int(e)), (i, j)):
            continue
        best = max(best, float(b_map[a, e]))
    return float(10 * np.log10(best)) if np.isfinite(best) and best > 0 else float("nan")


def _same_lobe(b_map: NDArray, peak: tuple[int, int], main: tuple[int, int]) -> bool:
    """Whether the local maximum ``peak`` is the one that steepest ascent from ``main`` reaches."""
    n_az, n_el = b_map.shape
    cur = main
    for _ in range(n_az * n_el):
        best, best_val = cur, b_map[cur]
        for da in (-1, 0, 1):
            for de in (-1, 0, 1):
                nxt = ((cur[0] + da) % n_az, cur[1] + de)
                if (da or de) and 0 <= nxt[1] < n_el and b_map[nxt] > best_val:
                    best, best_val = nxt, b_map[nxt]
        if best == cur:
            break
        cur = best
    return cur == peak


@dataclass(frozen=True)
class Result:
    """Beampattern figures of one array at one frequency."""

    width_az: float  # deg, median over the steering azimuths
    width_el: float  # deg, median over the steering azimuths
    psl_db: float  # dB, the worst (highest) sidelobe over the steering azimuths; NaN if none


def evaluate(
    mic_pos: NDArray,
    freq: float,
    el0_deg: float = STEER_EL_DEG,
    azimuths_deg: tuple[float, ...] = AZIMUTHS_DEG,
    c: float = sg.C,
) -> Result:
    """Widths and peak sidelobe level steering to elevation ``el0_deg`` at several azimuths."""
    dirs, (n_az, n_el) = hemisphere_grid()
    A = sg.steering(mic_pos, np.array([freq]), dirs, c)[:, 0, :]
    w_az, w_el, psl = [], [], []
    for az in azimuths_deg:
        u0 = g.unit_vector(np.deg2rad(az), np.deg2rad(el0_deg))
        w_az.append(lobe_width_deg(mic_pos, freq, u0, "east", c))
        w_el.append(lobe_width_deg(mic_pos, freq, u0, "north", c))
        b_map = _power(A, mic_pos, freq, u0, c).reshape(n_az, n_el)
        main = (int(round(az / GRID_STEP_DEG)) % n_az, int(round(el0_deg / GRID_STEP_DEG)))
        psl.append(peak_sidelobe_db(b_map, main))
    finite = [p for p in psl if np.isfinite(p)]
    return Result(
        float(np.median(w_az)), float(np.median(w_el)), max(finite) if finite else float("nan")
    )

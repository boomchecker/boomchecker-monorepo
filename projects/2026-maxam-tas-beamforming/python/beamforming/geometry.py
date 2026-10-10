"""Array geometry, direction conventions and search grids.

Conventions follow the report (``main.tex``, signal model):

* azimuth ``az`` is measured from the x axis, counter-clockwise, in [0, 2*pi),
* elevation ``el`` is measured from the horizontal plane, 0 = horizon, pi/2 = zenith,
* unit vector ``u = [cos(el) cos(az), cos(el) sin(az), sin(el)]``,
* only the upper hemisphere (el >= 0) is searched,
* the origin is the array centre (between the two boards for 2x8).

All angles in the API are radians unless a name ends with ``_deg``.
"""

from __future__ import annotations

from typing import Literal

import numpy as np
from numpy.typing import NDArray

Topology = Literal["1x8", "2x8", "2x8_rot"]
TOPOLOGIES: tuple[Topology, ...] = ("1x8", "2x8", "2x8_rot")

MICS_PER_RING = 8
ROTATION_2X8 = np.pi / MICS_PER_RING  # 22.5 deg, upper ring of "2x8_rot"


def ring(diameter: float, z: float, rotation: float = 0.0) -> NDArray[np.float64]:
    """Positions (8, 3) of one ring of 8 microphones, mic k at ``45 deg * k + rotation``."""
    angles = 2 * np.pi * np.arange(MICS_PER_RING) / MICS_PER_RING + rotation
    r = diameter / 2
    return np.stack([r * np.cos(angles), r * np.sin(angles), np.full(MICS_PER_RING, z)], axis=1)


def make_array(
    topology: Topology = "2x8_rot", diameter: float = 0.20, height: float = 0.07
) -> NDArray[np.float64]:
    """Microphone positions (M, 3) in metres.

    ``diameter`` is the diameter of the circle the MEMS microphones sit on (not the PCB
    outline). For 2x8 the lower board is at ``z = -height/2`` (channels 0..7, SAI SD_A) and
    the upper board at ``z = +height/2`` (channels 8..15, SAI SD_B).
    """
    if topology == "1x8":
        return ring(diameter, 0.0)
    if topology in ("2x8", "2x8_rot"):
        rotation = ROTATION_2X8 if topology == "2x8_rot" else 0.0
        return np.vstack([ring(diameter, -height / 2), ring(diameter, height / 2, rotation)])
    raise ValueError(f"unknown topology {topology!r}, expected one of {TOPOLOGIES}")


def unit_vector(az: NDArray[np.float64] | float, el: NDArray[np.float64] | float) -> NDArray:
    """Unit direction vectors (..., 3) for azimuth/elevation in radians."""
    az = np.asarray(az, dtype=float)
    el = np.asarray(el, dtype=float)
    return np.stack([np.cos(el) * np.cos(az), np.cos(el) * np.sin(az), np.sin(el)], axis=-1)


def to_angles(u: NDArray[np.float64]) -> tuple[NDArray[np.float64], NDArray[np.float64]]:
    """Azimuth in [0, 2*pi) and elevation in [-pi/2, pi/2] of unit vectors (..., 3)."""
    u = np.asarray(u, dtype=float)
    az = np.mod(np.arctan2(u[..., 1], u[..., 0]), 2 * np.pi)
    el = np.arcsin(np.clip(u[..., 2], -1.0, 1.0))
    return az, el


def angular_error_deg(u_est: NDArray[np.float64], u_true: NDArray[np.float64]) -> NDArray:
    """Great-circle angle in degrees between direction vectors (..., 3)."""
    cos = np.sum(np.asarray(u_est) * np.asarray(u_true), axis=-1)
    return np.rad2deg(np.arccos(np.clip(cos, -1.0, 1.0)))


def azimuth_error_deg(az_est: NDArray | float, az_true: NDArray | float) -> NDArray:
    """Absolute wrapped azimuth difference in degrees."""
    d = np.mod(np.asarray(az_est) - np.asarray(az_true) + np.pi, 2 * np.pi) - np.pi
    return np.abs(np.rad2deg(d))


def _check_step(step_deg: float) -> None:
    """The grid must hit the zenith and close the azimuth circle exactly."""
    if step_deg <= 0 or any(
        abs(full / step_deg - round(full / step_deg)) > 1e-9 for full in (90.0, 360.0)
    ):
        raise ValueError(f"grid step {step_deg} deg must divide 90 and 360 deg")


def coarse_grid(step_deg: float = 5.0) -> tuple[NDArray[np.float64], NDArray[np.float64]]:
    """Regular azimuth x elevation grid over the upper hemisphere.

    Returns flat ``(az, el)`` arrays of length ``n_az * n_el`` in azimuth-major order, so a
    power vector reshapes to a map with ``P.reshape(n_az, n_el)``. With 5 deg this is
    72 x 19 = 1368 directions (report, section 6). The zenith row repeats the same direction
    for every azimuth, which is harmless for an argmax search. ``step_deg`` must divide 90
    and 360.
    """
    n_az, n_el = grid_shape(step_deg)
    az = np.deg2rad(step_deg * np.arange(n_az))
    el = np.deg2rad(step_deg * np.arange(n_el))
    a, e = np.meshgrid(az, el, indexing="ij")
    return a.ravel(), e.ravel()


def grid_shape(step_deg: float = 5.0) -> tuple[int, int]:
    """``(n_az, n_el)`` of :func:`coarse_grid`."""
    _check_step(step_deg)
    return int(round(360.0 / step_deg)), int(round(90.0 / step_deg)) + 1


def tangent_basis(u0: NDArray[np.float64]) -> tuple[NDArray[np.float64], NDArray[np.float64]]:
    """Orthonormal east/north vectors of the tangent plane at ``u0``.

    At the zenith, where east is undefined, the x axis is used as east.
    """
    u0 = np.asarray(u0, dtype=float)
    east = np.cross([0.0, 0.0, 1.0], u0)
    norm = np.linalg.norm(east)
    east = np.array([1.0, 0.0, 0.0]) if norm < 1e-9 else east / norm
    north = np.cross(u0, east)
    return east, north


def fine_cap(
    u0: NDArray[np.float64], span_deg: float = 5.0, step_deg: float = 1.0
) -> NDArray[np.float64]:
    """Fine search directions (D, 3) around ``u0``.

    A fixed square of ``(2 n + 1)^2`` points with ``n = round(span / step)`` (11 x 11 for the
    defaults) on the tangent plane at ``u0`` (gnomonic projection). The cap is always symmetric
    and contains ``u0`` even when ``step`` does not divide ``span``. The spacing is ``step_deg``
    along both axes at any elevation, including the zenith, and azimuth wrap needs no special
    case.
    Points below the horizon are dropped.
    """
    n = int(round(span_deg / step_deg))  # always symmetric and includes the centre point
    offsets = np.deg2rad(step_deg * np.arange(-n, n + 1))
    dx, dy = np.meshgrid(offsets, offsets, indexing="ij")
    east, north = tangent_basis(u0)
    v = (
        np.asarray(u0)[None, :]
        + np.tan(dx.ravel())[:, None] * east[None, :]
        + np.tan(dy.ravel())[:, None] * north[None, :]
    )
    v /= np.linalg.norm(v, axis=1, keepdims=True)
    return v[v[:, 2] >= 0.0]


def random_directions(n: int, rng: np.random.Generator) -> NDArray[np.float64]:
    """``n`` unit vectors (n, 3) uniformly distributed over the upper hemisphere.

    The area element gives ``sin(el)`` uniform on [0, 1] and a uniform azimuth, so low
    elevations are as likely per solid angle as high ones (and rarer per elevation degree).
    """
    az = rng.uniform(0.0, 2 * np.pi, n)
    el = np.arcsin(rng.uniform(0.0, 1.0, n))
    return unit_vector(az, el)

"""Reference check against pyroomacoustics (pra) DOA algorithms.

pra works with colatitude (0 = zenith) on a grid it builds as the Cartesian product of the
given azimuths and colatitudes; this wrapper converts from the elevation convention of
:mod:`beamforming.geometry` and back, feeds pra the same STFT bins and the same coarse grid as
the own methods, and returns the grid maximum (no fine search, pra has none). The maximum is
taken from ``grid.values`` instead of pra's peak finder, which builds a convex-hull neighbour
graph first and would have to cope with the repeated zenith points of this grid.

Known pra issue: ``TOPS`` builds its transformation matrices ``Phi`` for the absolute bin
numbers (``pyroomacoustics/doa/tops.py``: ``Phi`` is sized ``nfft // 2 + 1``) but reads
``Phi[k]`` for ``k`` in ``range(num_freq - 1)``, a position in the list of used bins, while the
noise subspace ``W[k]`` belongs to bin ``freq_bins[k]``. With a band that does not start at
bin 0 it therefore uses the wrong frequencies. With the indexing fixed, the error on five
directions at 30 dB dropped from 5.5 to 11.9 deg to 0.6 to 3.1 deg (coarse grid
quantisation), so the failure says nothing about TOPS on a 3D array. TOPS is left out of the
report table for that reason.
"""

from __future__ import annotations

import numpy as np
from numpy.typing import NDArray

from . import geometry as g
from . import signals as sg
from .doa import DEFAULT, Config

PRA_METHODS: tuple[str, ...] = ("SRP", "MUSIC", "NormMUSIC", "TOPS", "CSSM", "WAVES")


def locate(
    name: str,
    X: NDArray,
    mic_pos: NDArray,
    cfg: Config = DEFAULT,
    bins: NDArray | None = None,
) -> tuple[float, float]:
    """Direction ``(azimuth, elevation)`` in rad of pra algorithm ``name`` on the coarse grid.

    ``X`` is the full STFT ``(M, nfft // 2 + 1, T)`` and ``bins`` the used bin indices
    (default: the localisation band, bins 10..64). Passing ``bins`` explicitly matters: pra's
    ``freq_range`` argument would drop the last bin.
    """
    import pyroomacoustics as pra

    if name not in PRA_METHODS:
        raise ValueError(f"unknown pra method {name!r}, expected one of {PRA_METHODS}")
    if bins is None:
        bins = sg.band_bins(cfg.fs, cfg.nfft, cfg.band)
    az, el = g.coarse_grid(cfg.coarse_step)
    doa = pra.doa.algorithms[name](
        np.asarray(mic_pos).T,
        cfg.fs,
        cfg.nfft,
        c=cfg.c,
        num_src=1,
        dim=3,
        azimuth=np.unique(az),
        colatitude=np.pi / 2 - np.unique(el),
    )
    doa.locate_sources(X, freq_bins=np.asarray(bins))
    best = int(np.argmax(doa.grid.values))
    return float(doa.grid.azimuth[best] % (2 * np.pi)), float(np.pi / 2 - doa.grid.colatitude[best])

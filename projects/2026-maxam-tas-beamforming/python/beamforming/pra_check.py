"""Reference check against pyroomacoustics (pra) DOA algorithms.

pra works with colatitude (0 = zenith) on a grid it builds as the Cartesian product of the
given azimuths and colatitudes; this wrapper converts from the elevation convention of
:mod:`beamforming.geometry` and back, feeds pra the same STFT bins and the same coarse grid as
the own methods, and returns the grid maximum (no fine search, pra has none). The maximum is
taken from ``grid.values`` instead of pra's peak finder, whose neighbour search on a grid with
repeated zenith points is not reliable.

Known pra issue: ``TOPS`` indexes its focusing matrices ``Phi[k]`` by position in the bin list
but builds them from absolute bin numbers (``pyroomacoustics/doa/tops.py``: ``Phi`` is sized
``nfft // 2 + 1`` and filled for all bins, then read with ``Phi[k]`` for ``k`` in
``range(num_freq - 1)``). With a band that does not start at bin 0 it therefore uses the
wrong frequencies, so its errors in the report table say nothing about TOPS on a 3D array.
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

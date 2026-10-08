"""Cramér-Rao bound on the direction of one far-field source (deterministic signal model).

Per STFT bin ``k`` and frame ``t`` the array sees ``x = a_k(u) s_kt + n`` with unknown
complex ``s_kt`` and white noise ``n ~ CN(0, sigma_k^2 I)``, independent over bins and frames
(Stoica and Nehorai, IEEE TASSP 1989). The Fisher information of ``u`` is the sum over bins
and frames of ``(2 |s_kt|^2 / sigma_k^2) Re[D^H P D]`` with ``D = da/du`` and
``P = I - a a^H / M``. The direction is parametrised by the east/north offsets ``(e_E, e_N)``
in the tangent plane at ``u`` (:func:`geometry.tangent_basis`); both are arc lengths, so the
bound has no singularity at the zenith and ``e_E`` is already cos(el)-weighted azimuth.
With ``D_m = j (2 pi f / c) rho_m a_m`` and ``rho_m = (r_m . east, r_m . north)`` the
projection removes the common part of ``rho``, leaving the scatter matrix of the
microphone positions in the tangent plane:

    FIM = sum_kt (2 |s_kt|^2 / sigma_k^2) (2 pi f_k / c)^2 sum_m (rho_m - mean rho)(...)^T

A planar array at the horizon has no scatter along ``north`` (its z axis), so the elevation
bound is infinite there. The bound is for unbiased estimators of the true direction; it
ignores the grid, the hemisphere constraint and outliers, so a real RMSE sits above it and
far above it below the threshold SNR.
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np
from numpy.typing import NDArray
from scipy import signal as sps

from . import geometry as g
from . import signals as sg

_SINGULAR = 1e12  # condition number above which the bound is reported as infinite


def tangent_scatter(mic_pos: NDArray, u: NDArray) -> NDArray[np.float64]:
    """Scatter matrix (2, 2) of the mic positions along (east, north) at direction ``u``."""
    east, north = g.tangent_basis(u)
    rho = np.asarray(mic_pos, dtype=float) @ np.stack([east, north], axis=1)
    rho = rho - rho.mean(axis=0)
    return rho.T @ rho


def fisher(
    clean_X: NDArray, mic_pos: NDArray, freqs: NDArray, u: NDArray, noise_var: float, c: float
) -> NDArray[np.float64]:
    """Fisher information (2, 2) in (east, north) arc length, units 1/rad^2.

    ``clean_X`` is the STFT ``(M, K, T)`` of the noise-free array signals, ``freqs`` the bin
    frequencies in Hz and ``noise_var`` the noise power ``E|n|^2`` of one channel in one bin.
    The source power ``|s_kt|^2`` is the matched-filter estimate ``|a^H x|^2 / M^2``.
    """
    a = sg.steering(mic_pos, freqs, np.asarray(u, dtype=float)[None, :], c)[:, :, 0]  # (M, K)
    s = np.einsum("mk,mkt->kt", a.conj(), clean_X) / a.shape[0]
    weight = 2.0 * np.sum(np.abs(s) ** 2, axis=1) / noise_var  # (K,) summed over frames
    kappa2 = (2 * np.pi * np.asarray(freqs) / c) ** 2
    return float(np.sum(weight * kappa2)) * tangent_scatter(mic_pos, u)


def covariance(fim: NDArray) -> NDArray[np.float64]:
    """Inverse of the Fisher information, all infinite when it is (nearly) singular."""
    if not np.all(np.isfinite(fim)) or np.linalg.cond(fim) > _SINGULAR:
        return np.full((2, 2), np.inf)
    return np.linalg.inv(fim)


@dataclass(frozen=True)
class Bound:
    """Bound on the RMS direction error in degrees of one trial."""

    azimuth: float  # arc length along the parallel, i.e. cos(el)-weighted azimuth
    elevation: float
    angular: float  # sqrt(trace), the bound on the great-circle error


def noise_var_per_bin(clean: NDArray, snr_db: float, nfft: int = sg.NFFT) -> float:
    """``E|n|^2`` of one channel in one STFT bin for white noise added at ``snr_db``.

    :func:`signals.add_noise` sets the sample variance to the array-mean power of the clean
    signal over ``10^(snr/10)``; the unnormalised Hann STFT scales it by ``sum(w^2)``.
    """
    sigma2 = sg.band_power(clean) / 10 ** (snr_db / 10)
    return float(sigma2 * np.sum(sps.get_window("hann", nfft) ** 2))


def bound(
    clean: NDArray,
    mic_pos: NDArray,
    u: NDArray,
    snr_db: float,
    nfft: int = sg.NFFT,
    hop: int = sg.HOP,
    band: tuple[float, float] = sg.BAND_HZ,
    fs: int = sg.FS,
    c: float = sg.C,
) -> Bound:
    """CRB of one simulated trial from its noise-free array signals ``clean`` (M, n)."""
    bins = sg.band_bins(fs, nfft, band)
    X = sg.stft(clean, nfft, hop)[:, bins]
    fim = fisher(
        X, mic_pos, sg.bin_freqs(bins, fs, nfft), u, noise_var_per_bin(clean, snr_db, nfft), c
    )
    cov = covariance(fim)
    rad = np.sqrt(np.array([cov[0, 0], cov[1, 1], cov[0, 0] + cov[1, 1]]))
    az, el, ang = np.rad2deg(rad)
    return Bound(float(az), float(el), float(ang))


def rms(bounds: list[Bound]) -> Bound:
    """RMS over trials, comparable to an RMSE over the same trials."""
    out = [
        float(np.sqrt(np.mean([getattr(b, f) ** 2 for b in bounds])))
        for f in ("azimuth", "elevation", "angular")
    ]
    return Bound(*out)

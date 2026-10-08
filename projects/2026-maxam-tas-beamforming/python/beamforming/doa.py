"""DOA methods over a common direction grid, plus GCC-PHAT with least squares.

Grid methods (DAS, MVDR, SRP-PHAT, MUSIC) are written as per-bin power maps ``(K, D)`` over
``D`` unit direction vectors. :func:`search` combines the bins non-coherently
(``sum_k w_k P_k``), first on the coarse hemisphere grid and then on a fine cap around its
maximum. For MVDR and MUSIC every bin map is normalised to its coarse-grid maximum
(``w_k = 1 / max_coarse P_k``) so a few strong harmonics cannot outvote the other bins; the
fine stage reuses the coarse constants, otherwise the two stages would weight bins
differently. DAS and SRP-PHAT are summed as they are.

Inputs ``X`` are STFT bins of the localisation band, shape ``(M, K, T)``, with the matching
frequencies ``freqs`` of shape ``(K,)``. Directions follow :mod:`beamforming.geometry`.
"""

from __future__ import annotations

from collections.abc import Callable
from dataclasses import dataclass

import numpy as np
from numpy.typing import NDArray

from . import geometry as g
from . import signals as sg

_TINY = 1e-12
BIN_WEIGHTINGS = ("max", "snr")


@dataclass(frozen=True)
class Config:
    """Processing parameters shared by all methods (defaults follow the report)."""

    fs: int = sg.FS
    c: float = sg.C
    nfft: int = sg.NFFT
    hop: int = sg.HOP
    band: tuple[float, float] = sg.BAND_HZ
    coarse_step: float = 5.0
    fine_span: float = 5.0
    fine_step: float = 1.0
    loading: float = 1e-2  # MVDR: relative diagonal loading eps in eps * tr(R) / M * I
    n_src: int = 1  # MUSIC: signal subspace dimension
    gcc_upsample: int = 16  # GCC-PHAT: zero-padded IFFT factor for sub-sample delays
    guard_bins: int = 0  # bins dropped at both band edges (Hann leakage from outside the band)
    bin_weighting: str = "max"  # "max": report weights; "snr": also scale each bin by 1 - N/P_k
    freq_smooth: int = 0  # MVDR, MUSIC: covariance averaged over +-freq_smooth neighbour bins
    noise_floor_hz: float = 3000.0  # "snr": noise floor is the median bin power above this

    def __post_init__(self) -> None:
        if self.bin_weighting not in BIN_WEIGHTINGS:
            raise ValueError(f"bin_weighting {self.bin_weighting!r} not in {BIN_WEIGHTINGS}")
        if self.guard_bins < 0 or self.freq_smooth < 0:
            raise ValueError("guard_bins and freq_smooth must not be negative")
        if self.bin_weighting == "snr" and not self.band[1] < self.noise_floor_hz < self.fs / 2:
            raise ValueError("noise_floor_hz must lie between the band and the Nyquist frequency")


DEFAULT = Config()

Bins = Callable[[NDArray, NDArray, NDArray, NDArray, Config], NDArray]


def _steer(mic_pos: NDArray, freqs: NDArray, dirs: NDArray, cfg: Config) -> NDArray:
    """Steering vectors arranged as (K, M, D) for batched matrix products."""
    return sg.steering(mic_pos, freqs, dirs, cfg.c).transpose(1, 0, 2)


def _smooth_bins(R: NDArray, n: int) -> NDArray:
    """Moving average of ``R`` (K, ...) over ``+-n`` neighbouring bins, shorter at the edges."""
    if n <= 0:
        return R
    K = R.shape[0]
    csum = np.concatenate([np.zeros((1, *R.shape[1:]), dtype=R.dtype), np.cumsum(R, axis=0)])
    k = np.arange(K)
    lo, hi = np.clip(k - n, 0, K), np.clip(k + n + 1, 0, K)
    return (csum[hi] - csum[lo]) / (hi - lo).reshape(-1, *([1] * (R.ndim - 1)))


def _covariance(X: NDArray, smooth: int = 0) -> NDArray:
    """Per-bin sample covariance (K, M, M) averaged over the T frames and ``+-smooth`` bins.

    Averaging neighbouring bins raises the rank above the T frames (5 for 100 ms). It treats
    the steering vector as constant over ``2 * smooth + 1`` bins (``smooth * df = 31 Hz`` per
    step), which is a small phase error for a 0.2 m array but not an exact focusing.
    """
    R = np.einsum("mkt,nkt->kmn", X, X.conj()) / X.shape[2]
    return _smooth_bins(R, smooth)


def das_bins(X: NDArray, mic_pos: NDArray, freqs: NDArray, dirs: NDArray, cfg: Config) -> NDArray:
    """Delay-and-sum output power ``a^H R a`` per bin (report, eq. das)."""
    A = _steer(mic_pos, freqs, dirs, cfg)
    Y = np.einsum("kmd,mkt->kdt", A.conj(), X)
    return np.mean(np.abs(Y) ** 2, axis=-1)


def srp_phat_bins(
    X: NDArray, mic_pos: NDArray, freqs: NDArray, dirs: NDArray, cfg: Config
) -> NDArray:
    """SRP-PHAT per bin: GCC-PHAT summed over all pairs at the steered delays (eq. srpphat).

    With the PHAT weight applied per channel and frame (``X / |X|``, as in pyroomacoustics),
    ``sum_{m<n} Re[a_m^* a_n p_m p_n^*] = (|a^H p|^2 - M) / 2``, which is evaluated here
    instead of looping over the 120 pairs.
    """
    M = X.shape[0]
    P = X / np.maximum(np.abs(X), _TINY)
    A = _steer(mic_pos, freqs, dirs, cfg)
    Y = np.einsum("kmd,mkt->kdt", A.conj(), P)
    return np.mean((np.abs(Y) ** 2 - M) / 2, axis=-1)


def mvdr_bins(X: NDArray, mic_pos: NDArray, freqs: NDArray, dirs: NDArray, cfg: Config) -> NDArray:
    """MVDR pseudo-spectrum ``1 / (a^H (R + eps tr(R)/M I)^-1 a)`` per bin (eq. mvdr)."""
    if cfg.loading <= 0:
        raise ValueError(f"MVDR needs diagonal loading > 0, got {cfg.loading}")
    M = X.shape[0]
    R = _covariance(X, cfg.freq_smooth)
    trace = np.trace(R, axis1=1, axis2=2).real
    R = R + (cfg.loading * trace / M)[:, None, None] * np.eye(M)
    A = _steer(mic_pos, freqs, dirs, cfg)
    quad = np.einsum("kmd,kmd->kd", A.conj(), np.linalg.solve(R, A)).real
    return 1.0 / np.maximum(quad, _TINY)


def music_bins(X: NDArray, mic_pos: NDArray, freqs: NDArray, dirs: NDArray, cfg: Config) -> NDArray:
    """MUSIC pseudo-spectrum per bin (eq. music).

    ``a^H E_n E_n^H a = ||a||^2 - ||E_s^H a||^2`` with ``||a||^2 = M``, so only the
    ``n_src`` dominant eigenvectors are needed.
    """
    M = X.shape[0]
    _, vecs = np.linalg.eigh(_covariance(X, cfg.freq_smooth))
    Es = vecs[:, :, -cfg.n_src :]
    A = _steer(mic_pos, freqs, dirs, cfg)
    proj = np.sum(np.abs(np.einsum("kms,kmd->ksd", Es.conj(), A)) ** 2, axis=1)
    return 1.0 / np.maximum(M - proj, _TINY * M)


@dataclass(frozen=True)
class GridMethod:
    """A per-bin power map plus whether bins are normalised before they are summed."""

    name: str
    bins: Bins
    normalize: bool


GRID_METHODS: dict[str, GridMethod] = {
    "das": GridMethod("das", das_bins, normalize=False),
    "mvdr": GridMethod("mvdr", mvdr_bins, normalize=True),
    "srp_phat": GridMethod("srp_phat", srp_phat_bins, normalize=False),
    "music": GridMethod("music", music_bins, normalize=True),
}


def combine(bins: NDArray, weights: NDArray | None = None) -> NDArray:
    """Non-coherent sum over bins, ``P_d = sum_k w_k P_{k,d}``."""
    if weights is None:
        return bins.sum(axis=0)
    return weights @ bins


def bin_weights(method: GridMethod, coarse_bins: NDArray) -> NDArray | None:
    """Per-bin weights ``1 / max_d P_k`` from the coarse map, or ``None`` if not normalised."""
    if not method.normalize:
        return None
    return 1.0 / np.maximum(coarse_bins.max(axis=1), _TINY)


def snr_gain(X_full: NDArray, bins: NDArray, cfg: Config = DEFAULT) -> NDArray:
    """Per-bin gain ``max(0, 1 - N / P_k)`` for ``cfg.bin_weighting == "snr"``.

    ``P_k`` is the mean power of bin ``k`` over channels and frames and ``N`` the median bin
    power above ``cfg.noise_floor_hz``, where the drone has little energy and the noise is
    white. The gain is 0 for a noise-only bin and tends to 1 for a strong one. ``X_full`` is
    the whole STFT ``(M, F, T)``; ``bins`` are the indices of the localisation band.
    """
    power = np.mean(np.abs(X_full) ** 2, axis=(0, 2))
    freqs = sg.bin_freqs(np.arange(len(power)), cfg.fs, cfg.nfft)
    noise = float(np.median(power[freqs >= cfg.noise_floor_hz]))
    return np.maximum(0.0, 1.0 - noise / np.maximum(power[bins], _TINY))


def _bin_gain(method: GridMethod, coarse: NDArray, gain: NDArray | None) -> NDArray | None:
    """Bin weights of a method, times the optional per-bin ``gain``."""
    weights = bin_weights(method, coarse)
    if gain is None:
        return weights
    return gain if weights is None else weights * gain


def power_map(
    method: str,
    X: NDArray,
    mic_pos: NDArray,
    freqs: NDArray,
    cfg: Config = DEFAULT,
    step: float | None = None,
    gain: NDArray | None = None,
) -> NDArray:
    """Combined power over the coarse hemisphere grid, shape ``(n_az, n_el)``."""
    m = GRID_METHODS[method]
    step = cfg.coarse_step if step is None else step
    az, el = g.coarse_grid(step)
    bins = m.bins(X, mic_pos, freqs, g.unit_vector(az, el), cfg)
    return combine(bins, _bin_gain(m, bins, gain)).reshape(g.grid_shape(step))


def search(
    method: str,
    X: NDArray,
    mic_pos: NDArray,
    freqs: NDArray,
    cfg: Config = DEFAULT,
    gain: NDArray | None = None,
) -> tuple[float, float]:
    """Coarse grid maximum, then a fine cap around it; returns ``(azimuth, elevation)`` in rad.

    ``gain`` (K,) is an optional extra weight per bin (see :func:`snr_gain`).
    """
    m = GRID_METHODS[method]
    az, el = g.coarse_grid(cfg.coarse_step)
    dirs = g.unit_vector(az, el)
    coarse = m.bins(X, mic_pos, freqs, dirs, cfg)
    weights = _bin_gain(m, coarse, gain)
    u0 = dirs[int(np.argmax(combine(coarse, weights)))]
    cap = g.fine_cap(u0, cfg.fine_span, cfg.fine_step)
    fine = combine(m.bins(X, mic_pos, freqs, cap, cfg), weights)
    a, e = g.to_angles(cap[int(np.argmax(fine))])
    return float(a), float(e)


def gcc_delays(x: NDArray, mic_pos: NDArray, cfg: Config = DEFAULT) -> tuple[NDArray, NDArray]:
    """GCC-PHAT delay ``tau_ij = (r_i - r_j)^T u / c`` of every microphone pair ``i < j``.

    Returns ``(diff, tau)`` with ``diff = r_i - r_j`` of shape (P, 3) and ``tau`` in seconds.
    PHAT is applied inside the localisation band on the whole segment (zero-padded 2x). The
    correlation is sampled ``gcc_upsample`` times finer by a zero-padded inverse FFT, and the
    peak is searched only inside the physically possible lag ``|tau| <= |r_i - r_j| / c``.
    ``IFFT[X_i X_j^*]`` peaks at ``-tau_ij``.
    """
    x = np.atleast_2d(np.asarray(x, dtype=float))
    M, n = x.shape
    nz = 2 * n
    up = cfg.gcc_upsample
    spec = np.fft.rfft(x, n=nz, axis=1)
    freqs = np.fft.rfftfreq(nz, 1 / cfg.fs)
    in_band = (freqs >= cfg.band[0]) & (freqs <= cfg.band[1])

    i_idx, j_idx = np.triu_indices(M, k=1)
    cross = spec[i_idx] * spec[j_idx].conj()
    cross = np.where(in_band, cross / np.maximum(np.abs(cross), _TINY), 0.0)
    cc = np.fft.irfft(cross, n=nz * up, axis=1)  # lag l at index l mod nz*up, step 1/(fs*up)

    diff = np.asarray(mic_pos)[i_idx] - np.asarray(mic_pos)[j_idx]
    max_lag = np.ceil(np.linalg.norm(diff, axis=1) / cfg.c * cfg.fs * up).astype(int) + 1
    lags = np.empty(len(i_idx))
    for p in range(len(i_idx)):
        window = np.arange(-max_lag[p], max_lag[p] + 1)
        lags[p] = window[int(np.argmax(cc[p, window % (nz * up)]))]
    return diff, -lags / (cfg.fs * up)


def direction_from_delays(diff: NDArray, tau: NDArray, c: float = sg.C) -> NDArray:
    """Least-squares unit direction from ``diff^T u = c tau``, clipped to the upper hemisphere.

    A planar array has a zero z column in ``diff``; then ``u_z`` follows from ``|u| = 1``.
    A solution below the horizon is projected onto it (``u_z = 0``).
    """
    diff = np.asarray(diff, dtype=float)
    rhs = c * np.asarray(tau, dtype=float)
    if np.linalg.matrix_rank(diff) < 3:  # planar: z column is zero
        uxy, *_ = np.linalg.lstsq(diff[:, :2], rhs, rcond=None)
        uz = np.sqrt(max(0.0, 1.0 - float(uxy @ uxy)))
        u = np.array([uxy[0], uxy[1], uz])
    else:
        u, *_ = np.linalg.lstsq(diff, rhs, rcond=None)
    if u[2] < 0:
        u = np.array([u[0], u[1], 0.0])
    return u / np.linalg.norm(u)


def gcc_phat_ls(x: NDArray, mic_pos: NDArray, cfg: Config = DEFAULT) -> tuple[float, float]:
    """GCC-PHAT delays of all pairs, then a least-squares direction (no grid).

    See :func:`gcc_delays` and :func:`direction_from_delays`; for 16 microphones the linear
    system has 120 equations.
    """
    diff, tau = gcc_delays(x, mic_pos, cfg)
    a, e = g.to_angles(direction_from_delays(diff, tau, cfg.c))
    return float(a), float(e)


METHODS: tuple[str, ...] = (*GRID_METHODS, "gcc_phat_ls")


def localize(
    method: str,
    x: NDArray,
    mic_pos: NDArray,
    cfg: Config = DEFAULT,
) -> tuple[float, float]:
    """Direction ``(azimuth, elevation)`` in radians from time-domain array signals ``x``.

    The signal is scaled to unit RMS first, so the absolute floors inside the methods do not
    depend on the input scale (float audio in [-1, 1) and raw integer samples alike). A silent
    segment has no direction and raises ``ValueError``.
    """
    if method != "gcc_phat_ls" and method not in GRID_METHODS:
        raise ValueError(f"unknown method {method!r}, expected one of {METHODS}")
    x = np.atleast_2d(np.asarray(x, dtype=float))
    rms = float(np.sqrt(np.mean(x**2)))
    if not np.isfinite(rms) or rms == 0.0:
        raise ValueError("silent or non-finite segment: no direction can be estimated")
    x = x / rms
    if method == "gcc_phat_ls":
        return gcc_phat_ls(x, mic_pos, cfg)
    X = sg.stft(x, cfg.nfft, cfg.hop)
    bins = sg.band_bins(cfg.fs, cfg.nfft, cfg.band)
    if cfg.guard_bins:
        bins = bins[cfg.guard_bins : len(bins) - cfg.guard_bins]
        if len(bins) == 0:
            raise ValueError(f"guard_bins={cfg.guard_bins} leaves no bin in the band")
    gain = snr_gain(X, bins, cfg) if cfg.bin_weighting == "snr" else None
    return search(method, X[:, bins], mic_pos, sg.bin_freqs(bins, cfg.fs, cfg.nfft), cfg, gain)

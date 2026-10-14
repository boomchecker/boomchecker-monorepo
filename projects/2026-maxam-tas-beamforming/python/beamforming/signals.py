"""Signal chain: steering vectors, free-field propagation, noise, STFT, decimation.

Model (report, eq. ``model``): microphone m at ``r_m`` receives a plane wave from direction
``u`` ahead of the origin by ``tau_m = r_m^T u / c``, so ``x_m(t) = s(t + tau_m)`` and
``X_m(f) = S(f) exp(+j 2 pi f tau_m)``. The same :func:`steering` is used to synthesise the
array signals and by every DOA method.
"""

from __future__ import annotations

import numpy as np
from numpy.typing import NDArray
from scipy import signal as sps

FS = 16_000
C = 343.0
NFFT = 512
HOP = 256
BAND_HZ = (300.0, 2000.0)
SEGMENT = 1600  # 100 ms at 16 kHz
MARGIN = 1024  # context kept on both sides of a segment during propagation


def delays(mic_pos: NDArray, dirs: NDArray, c: float = C) -> NDArray[np.float64]:
    """Arrival advance ``tau_m = r_m^T u / c`` in seconds, shape (M, D) for dirs (D, 3)."""
    return np.asarray(mic_pos) @ np.atleast_2d(dirs).T / c


def steering(mic_pos: NDArray, freqs: NDArray, dirs: NDArray, c: float = C) -> NDArray:
    """Steering vectors ``a_m(f, u) = exp(+j 2 pi f tau_m(u))``, shape (M, F, D)."""
    tau = delays(mic_pos, dirs, c)
    return np.exp(2j * np.pi * np.asarray(freqs)[None, :, None] * tau[:, None, :])


def propagate(
    s: NDArray, mic_pos: NDArray, u: NDArray, fs: int = FS, c: float = C
) -> NDArray[np.float64]:
    """Fractionally delay a mono signal into all microphones in the frequency domain.

    The shift is circular, so callers keep a margin of real signal around the samples they
    use (see :func:`observe`).
    """
    s = np.asarray(s, dtype=float)
    n = s.shape[-1]
    freqs = np.fft.rfftfreq(n, 1 / fs)
    a = steering(mic_pos, freqs, np.asarray(u, dtype=float)[None, :], c)[:, :, 0]
    return np.fft.irfft(np.fft.rfft(s)[None, :] * a, n=n, axis=1)


def observe(
    clip: NDArray,
    mic_pos: NDArray,
    u: NDArray,
    offset: int,
    n: int = SEGMENT,
    fs: int = FS,
    c: float = C,
    margin: int = MARGIN,
) -> NDArray[np.float64]:
    """Array signals (M, n) of ``clip[offset:offset+n]`` arriving from direction ``u``.

    Propagates ``margin`` extra samples on both sides and crops them, so the circular shift
    in :func:`propagate` does not wrap into the returned samples.
    """
    if offset < margin or offset + n + margin > len(clip):
        raise ValueError(f"segment [{offset}, {offset + n}) needs {margin} samples of margin")
    x = propagate(clip[offset - margin : offset + n + margin], mic_pos, u, fs, c)
    return x[:, margin : margin + n]


def band_power(x: NDArray, fs: int = FS, band: tuple[float, float] | None = None) -> float:
    """Mean power per sample and channel, optionally restricted to a frequency band."""
    x = np.atleast_2d(x)
    if band is None:
        return float(np.mean(x**2))
    n = x.shape[-1]
    spec = np.abs(np.fft.rfft(x, axis=-1)) ** 2
    freqs = np.fft.rfftfreq(n, 1 / fs)
    weight = np.full(freqs.shape, 2.0)  # one-sided spectrum, DC and Nyquist counted once
    weight[0] = 1.0
    if n % 2 == 0:
        weight[-1] = 1.0
    mask = (freqs >= band[0]) & (freqs <= band[1])
    return float(np.sum(spec[:, mask] * weight[mask]) / (n * n * x.shape[0]))


def add_noise(
    x: NDArray,
    snr_db: float,
    rng: np.random.Generator,
    snr_band: tuple[float, float] | None = None,
    fs: int = FS,
) -> NDArray[np.float64]:
    """Add independent white Gaussian noise to every channel at the given SNR.

    The SNR is the mean signal power over the array divided by the noise power, both
    measured on the actual realisation (wideband by default, or inside ``snr_band``).
    """
    x = np.asarray(x, dtype=float)
    noise = rng.standard_normal(x.shape)
    p_signal = band_power(x, fs, snr_band)
    p_noise = band_power(noise, fs, snr_band)
    noise *= np.sqrt(p_signal / (p_noise * 10 ** (snr_db / 10)))
    return x + noise


def stft(x: NDArray, nfft: int = NFFT, hop: int = HOP) -> NDArray[np.complex128]:
    """Multichannel STFT (M, F, T) with a periodic Hann window and no padding.

    1600 samples with the defaults give T = 5 frames (report, section 6).
    """
    x = np.atleast_2d(np.asarray(x, dtype=float))
    n_frames = 1 + (x.shape[1] - nfft) // hop
    if n_frames < 1:
        raise ValueError(f"signal of {x.shape[1]} samples is shorter than nfft={nfft}")
    window = sps.get_window("hann", nfft)
    idx = np.arange(nfft)[None, :] + hop * np.arange(n_frames)[:, None]
    frames = x[:, idx] * window  # (M, T, nfft)
    return np.fft.rfft(frames, axis=-1).transpose(0, 2, 1)


def band_bins(
    fs: int = FS, nfft: int = NFFT, band: tuple[float, float] = BAND_HZ
) -> NDArray[np.int64]:
    """STFT bins whose centre frequency lies inside ``band`` (inclusive).

    300 Hz to 2 kHz at 512 points and 16 kHz is bins 10..64. A band edge between two bins
    rounds inwards, so no used bin is outside the band.
    """
    df = fs / nfft
    lo = int(np.ceil(band[0] / df - 1e-9))
    hi = int(np.floor(band[1] / df + 1e-9))
    return np.arange(lo, hi + 1)


def bin_freqs(bins: NDArray, fs: int = FS, nfft: int = NFFT) -> NDArray[np.float64]:
    """Centre frequencies in Hz of STFT bins."""
    return np.asarray(bins) * fs / nfft


def decimation_filter(fs_in: int = 48_000, ripple_db: float = 60.0) -> NDArray[np.float64]:
    """Linear-phase Kaiser FIR for 48 to 16 kHz: passband to 7.2 kHz, stopband from 8 kHz."""
    width = 800.0 / (fs_in / 2)
    numtaps, beta = sps.kaiserord(ripple_db, width)
    numtaps |= 1  # odd length, integer group delay
    return sps.firwin(numtaps, 7600.0, window=("kaiser", beta), fs=fs_in)


def decimate_48k_to_16k(x: NDArray) -> NDArray[np.float64]:
    """FIR low-pass and 3:1 downsampling along the last axis (reference for the FW chain).

    The output is delayed by ``(len(decimation_filter()) - 1) / 2`` input samples.
    """
    return sps.upfirdn(decimation_filter(), np.asarray(x, dtype=float), up=1, down=3, axis=-1)

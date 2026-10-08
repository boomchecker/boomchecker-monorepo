"""Band spectrograms for the 2026-10-07 comparison: MFE (mel) and GS (gammatone), three FFTs.

Offline only - there is no C side. Six front-ends, named `<kind><n_fft / 1024>k`:

    mfe1k mfe2k mfe4k   64 mel bands (Slaney scale, the librosa default), 50 Hz - 8 kHz
    gs1k  gs2k  gs4k    64 fourth-order gammatone filters on the ERB scale, 50 Hz - 8 kHz

Each is the log of the mean power spectral density in each band:
ln(sum_k W[b, k] P[k] + SPEC_LOG_OFFSET), every row of W summing to one, with
P = |rfft(w x)|^2 / sum(w^2) for a periodic Hann window w - so a band reads
the same level whatever the FFT length and whatever its own width.

The hop stays 512 samples and the analysis window of N samples ENDS where the
board's 1024-sample frame ends: frame f covers samples [512 f + 1024 - N,
512 f + 1024). A longer window therefore looks further back, as it would on a
board that keeps the last N samples, and the frame grid is the frame cache's
(frame f here is frame f there, same count). Where a clip has no history yet
(its first frames when N > 1024) the signal is mirrored in front of sample 0
rather than zero-padded: zeros would give every short public clip (HF 0.5 s,
DAD 1 s) a quiet ramp no field recording has, a source fingerprint a network
could learn.

Gammatone weights: |H(f)|^2 = (1 + ((f - fc) / b)^2)^-4 with b = 1.019 ERB(fc),
Glasberg & Moore's ERB(f) = 24.7 (4.37 f / 1000 + 1), centres equally spaced in
ERB-rate - the construction of the 2026-10-06 GTCC experiment. Computed from the
FFT magnitude it keeps the gammatone's frequency selectivity, not its time-domain
fine structure: the cheap version the board could afford.
"""

from __future__ import annotations

from dataclasses import dataclass
from functools import lru_cache

import numpy as np

from boomdetect_train.dsp.mfcc import HOP, SAMPLE_RATE_HZ, WINDOW_SIZE, n_frames

SPEC_N_BANDS = 64
SPEC_FMIN_HZ = 50.0
SPEC_FMAX_HZ = 8000.0
SPEC_LOG_OFFSET = 1.0e-12  # on power; a silent 12-bit mic sits near 1e-8
GT_ORDER = 4
GT_WEIGHT_FLOOR = 1.0e-4  # gammatone power response below -40 dB is cut: the tails are long
CHUNK_FRAMES = 2048  # frames per FFT batch, to bound memory on long recordings


@dataclass(frozen=True)
class SpecFrontend:
    name: str
    kind: str  # "mfe" | "gs"
    n_fft: int


SPEC_FRONTENDS: tuple[SpecFrontend, ...] = tuple(
    SpecFrontend(f"{kind}{n // 1024}k", kind, n)
    for kind in ("mfe", "gs")
    for n in (1024, 2048, 4096)
)
SPEC_FE_NAMES: tuple[str, ...] = tuple(fe.name for fe in SPEC_FRONTENDS)
SPEC_BY_NAME = {fe.name: fe for fe in SPEC_FRONTENDS}


# --- filterbanks ----------------------------------------------------------------


def erb_hz(f_hz):
    return 24.7 * (4.37 * np.asarray(f_hz, dtype=np.float64) / 1000.0 + 1.0)


def erb_rate(f_hz):
    return 21.4 * np.log10(4.37 * np.asarray(f_hz, dtype=np.float64) / 1000.0 + 1.0)


def erb_rate_to_hz(e):
    return (10.0 ** (np.asarray(e, dtype=np.float64) / 21.4) - 1.0) * 1000.0 / 4.37


def hz_to_mel_slaney(f_hz):
    """Slaney's mel: linear (200/3 Hz per mel) below 1 kHz, logarithmic above."""
    f = np.asarray(f_hz, dtype=np.float64)
    lin = f / (200.0 / 3.0)
    logstep = np.log(6.4) / 27.0
    return np.where(f >= 1000.0, 15.0 + np.log(np.maximum(f, 1e-9) / 1000.0) / logstep, lin)


def mel_slaney_to_hz(m):
    m = np.asarray(m, dtype=np.float64)
    logstep = np.log(6.4) / 27.0
    return np.where(m >= 15.0, 1000.0 * np.exp(logstep * (m - 15.0)), m * (200.0 / 3.0))


def _bin_freqs(n_fft: int) -> np.ndarray:
    return np.arange(n_fft // 2 + 1) * (SAMPLE_RATE_HZ / n_fft)


def _normalise_rows(w: np.ndarray, centres: np.ndarray, f: np.ndarray) -> np.ndarray:
    """Rows sum to one; a row the bins missed entirely takes the bin nearest its centre."""
    w = w.copy()
    w[:, 0] = 0.0  # DC: the PDM offset, as everywhere else
    for b in np.flatnonzero(w.sum(axis=1) <= 0.0):
        w[b, int(np.argmin(np.abs(f[1:] - centres[b]))) + 1] = 1.0
    return w / w.sum(axis=1, keepdims=True)


@lru_cache(maxsize=8)
def mel_matrix(n_fft: int, n_bands: int = SPEC_N_BANDS) -> np.ndarray:
    """(n_bands, n_fft/2 + 1) triangular mel weights, rows summing to one."""
    f = _bin_freqs(n_fft)
    edges = mel_slaney_to_hz(
        np.linspace(hz_to_mel_slaney(SPEC_FMIN_HZ), hz_to_mel_slaney(SPEC_FMAX_HZ), n_bands + 2)
    )
    lo, mid, hi = edges[:-2], edges[1:-1], edges[2:]
    up = (f[None, :] - lo[:, None]) / (mid - lo)[:, None]
    down = (hi[:, None] - f[None, :]) / (hi - mid)[:, None]
    w = np.maximum(0.0, np.minimum(up, down))
    return _normalise_rows(w, mid, f)


@lru_cache(maxsize=8)
def gammatone_matrix(n_fft: int, n_bands: int = SPEC_N_BANDS) -> np.ndarray:
    """(n_bands, n_fft/2 + 1) gammatone power weights, rows summing to one."""
    f = _bin_freqs(n_fft)
    fc = erb_rate_to_hz(np.linspace(erb_rate(SPEC_FMIN_HZ), erb_rate(SPEC_FMAX_HZ), n_bands))
    b = 1.019 * erb_hz(fc)
    w = (1.0 + ((f[None, :] - fc[:, None]) / b[:, None]) ** 2) ** (-float(GT_ORDER))
    w = np.where(w < GT_WEIGHT_FLOOR, 0.0, w)
    return _normalise_rows(w, fc, f)


def band_matrix(fe: SpecFrontend) -> np.ndarray:
    return mel_matrix(fe.n_fft) if fe.kind == "mfe" else gammatone_matrix(fe.n_fft)


def band_centres_hz(fe: SpecFrontend) -> np.ndarray:
    """Centre of every band (for plots and the write-up)."""
    if fe.kind == "mfe":
        edges = mel_slaney_to_hz(
            np.linspace(
                hz_to_mel_slaney(SPEC_FMIN_HZ), hz_to_mel_slaney(SPEC_FMAX_HZ), SPEC_N_BANDS + 2
            )
        )
        return edges[1:-1]
    return erb_rate_to_hz(np.linspace(erb_rate(SPEC_FMIN_HZ), erb_rate(SPEC_FMAX_HZ), SPEC_N_BANDS))


@lru_cache(maxsize=4)
def hann_periodic(n: int) -> np.ndarray:
    return 0.5 - 0.5 * np.cos(2.0 * np.pi * np.arange(n) / n)


# --- the spectrograms -------------------------------------------------------------


def _power_frames(x16k: np.ndarray, n_fft: int, n_frames_: int) -> np.ndarray:
    """(F, n_fft/2 + 1) power spectral density of every frame, windows ending with the board's."""
    pad = n_fft - WINDOW_SIZE
    x = np.asarray(x16k, dtype=np.float64)
    if pad > 0:
        if x.shape[0] > 1:
            x = np.pad(x, (pad, 0), mode="reflect")
        else:
            x = np.pad(x, (pad, 0))
    win = hann_periodic(n_fft)
    norm = float((win * win).sum())
    view = np.lib.stride_tricks.sliding_window_view(x, n_fft)[::HOP][:n_frames_]
    out = np.empty((n_frames_, n_fft // 2 + 1), dtype=np.float64)
    for s in range(0, n_frames_, CHUNK_FRAMES):
        spec = np.fft.rfft(view[s : s + CHUNK_FRAMES] * win, axis=1)
        out[s : s + CHUNK_FRAMES] = (spec.real**2 + spec.imag**2) / norm
    return out


def spec_frames(
    x16k: np.ndarray, names: tuple[str, ...] | list[str] | None = None
) -> dict[str, np.ndarray]:
    """{front-end name: (F, SPEC_N_BANDS) float32 log band power} of a 16 kHz signal.

    F is the frame cache's count for the same signal (mfcc.n_frames). One FFT per
    length serves both kinds.
    """
    wanted = [SPEC_BY_NAME[n] for n in (names or SPEC_FE_NAMES)]
    nf = n_frames(np.asarray(x16k).shape[0])
    out: dict[str, np.ndarray] = {}
    for n_fft in sorted({fe.n_fft for fe in wanted}):
        if nf == 0:
            p = np.empty((0, n_fft // 2 + 1))
        else:
            p = _power_frames(x16k, n_fft, nf)
        for fe in wanted:
            if fe.n_fft != n_fft:
                continue
            out[fe.name] = np.log(p @ band_matrix(fe).T + SPEC_LOG_OFFSET).astype(np.float32)
    return out


def frame_seconds(n_fft: int) -> float:
    """How much audio one frame of a front-end looks at."""
    return n_fft / SAMPLE_RATE_HZ

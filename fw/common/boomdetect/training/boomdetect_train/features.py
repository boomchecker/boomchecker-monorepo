"""Feature layouts: what a window of frames becomes before a model sees it.

Each layout has a numeric id that the C extractor and the C model both declare
(include/extractor.h), so that a model can never be handed a vector in a
different order than it was trained on. The ids and the exact arithmetic here
are the specification the C extractors under src/ implement; the parity tests
hold the two together.

Layout 1, "stats" (52): [mean, std, dmean, cmax] x 13 MFCC coefficients. What
    the deployed model reads. Population std; dmean is the mean absolute
    frame-to-frame delta; cmax is max minus mean.
Layout 2, "stats_spectral" (69): layout 1, then mean and std over the window of
    eight per-frame spectral scalars, then the mean absolute frame-to-frame
    change of the log-mel vector. The scalars are the things that separate a
    rotor from a hum: how much energy sits above 4 kHz, how flat the spectrum
    is, how strongly a harmonic comb fits and where, and whether that pitch
    holds still across the window.
Layout 3, "logmel" (280): the 14 x 20 log-mel patch minus its own mean, frame
    major. The CNN input. Subtracting the window mean is what makes it
    level-invariant, the same role feature 0 being skipped plays for the MLPs.
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np

from boomdetect_train.dsp.mfcc import HOP, N_MELS, N_MFCC, SAMPLE_RATE_HZ, WINDOW_SIZE
from boomdetect_train.dsp.windows import ACCUM_FRAMES

LAYOUT_STATS = 1
LAYOUT_STATS_SPECTRAL = 2
LAYOUT_LOGMEL = 3

N_STATS = 4 * N_MFCC  # 52
N_SCALARS = 8
N_STATS_SPECTRAL = N_STATS + 2 * N_SCALARS + 1  # 69
N_LOGMEL_PATCH = ACCUM_FRAMES * N_MELS  # 280

SCALAR_NAMES = (
    "hi_ratio",  # power 4..8 kHz over power 0..8 kHz
    "mid_ratio",  # power 1..4 kHz over total
    "centroid",  # spectral centroid, normalised by 8 kHz
    "flatness",  # geometric over arithmetic mean of the power bins
    "rolloff",  # bin below which 85 % of the power lies, normalised by 512
    "crest",  # max magnitude over mean magnitude
    "harmonicity",  # best harmonic comb sum over the total magnitude
    "f0",  # frequency of that comb, normalised by 400 Hz
)

# Guards used identically in the C. Power sums of a silent frame are exactly
# zero on both sides, so these decide the value there rather than round it.
POWER_EPS = 1.0e-12
BIN_HZ = SAMPLE_RATE_HZ / WINDOW_SIZE  # 15.625
N_BINS = WINDOW_SIZE // 2 + 1  # 513
BIN_1K = int(1000 / BIN_HZ)  # 64
BIN_4K = int(4000 / BIN_HZ)  # 256
ROLLOFF_FRACTION = 0.85
F0_MIN_HZ = 60.0
F0_MAX_HZ = 400.0
F0_STEPS_PER_OCTAVE = 24
N_HARMONICS = 8


def f0_candidates() -> np.ndarray:
    """Comb fundamentals, 1/24 octave apart from 60 Hz to just under 400 Hz (66 of them).

    Returned as float32-exact values: src/frame_scalars.c carries the same 66
    numbers as literals, so both sides interpolate at identical frequencies.
    """
    n = int(np.floor(F0_STEPS_PER_OCTAVE * np.log2(F0_MAX_HZ / F0_MIN_HZ))) + 1
    return (
        (F0_MIN_HZ * 2.0 ** (np.arange(n) / F0_STEPS_PER_OCTAVE))
        .astype(np.float32)
        .astype(np.float64)
    )


def stats52(mfcc_win: np.ndarray) -> np.ndarray:
    """Layout 1 from a (n_frames, 13) block, exactly as src/extractor_stats.c."""
    m = np.asarray(mfcc_win, dtype=np.float32)
    if m.ndim != 2 or m.shape[1] != N_MFCC:
        raise ValueError(f"expected (frames, {N_MFCC}), got {m.shape}")
    mean = m.mean(axis=0, dtype=np.float32)
    std = np.sqrt(((m - mean) ** 2).mean(axis=0, dtype=np.float32))
    if m.shape[0] > 1:
        dmean = np.abs(np.diff(m, axis=0)).mean(axis=0, dtype=np.float32)
    else:
        dmean = np.zeros(N_MFCC, dtype=np.float32)
    cmax = m.max(axis=0) - mean
    return np.concatenate([mean, std, dmean, cmax]).astype(np.float32)


def _interp_mag(mag: np.ndarray, freqs_hz: np.ndarray) -> np.ndarray:
    """Linear interpolation of a magnitude spectrum at arbitrary frequencies."""
    pos = np.asarray(freqs_hz) / BIN_HZ
    k = np.floor(pos).astype(np.int64)
    frac = pos - k
    k = np.clip(k, 0, N_BINS - 2)
    return mag[k] * (1.0 - frac) + mag[k + 1] * frac


def frame_scalars(mag: np.ndarray) -> np.ndarray:
    """The eight per-frame spectral scalars from one (513,) magnitude spectrum.

    Bin 0 (DC) is excluded from every sum: the PDM chain leaves an offset there
    that says nothing about the sound.
    """
    mag = np.asarray(mag, dtype=np.float64)
    m = mag[1:N_BINS]  # bins 1..512
    p = m * m
    total = float(p.sum()) + POWER_EPS

    hi = float(p[BIN_4K - 1 :].sum()) / total  # bins 256..512
    mid = float(p[BIN_1K - 1 : BIN_4K - 1].sum()) / total  # bins 64..255
    k = np.arange(1, N_BINS, dtype=np.float64)
    centroid = float((k * p).sum()) / total * BIN_HZ / (SAMPLE_RATE_HZ / 2)
    flatness = float(np.exp(np.mean(np.log(p + POWER_EPS)))) / (float(p.mean()) + POWER_EPS)
    cum = np.cumsum(p)
    roll_idx = int(np.searchsorted(cum, ROLLOFF_FRACTION * cum[-1])) + 1  # bin number
    rolloff = roll_idx / (N_BINS - 1)
    msum = float(m.sum()) + POWER_EPS
    crest = float(m.max()) / (msum / (N_BINS - 1))

    cands = f0_candidates()
    best, best_f0 = 0.0, cands[0]
    for f0 in cands:
        h = np.arange(1, N_HARMONICS + 1) * f0
        h = h[h <= SAMPLE_RATE_HZ / 2]
        comb = float(_interp_mag(mag, h).sum())
        if comb > best:
            best, best_f0 = comb, f0
    # A silent frame has no comb: report 0 and the lowest candidate rather than
    # 0/eps noise, and the same on the C side.
    harmonicity = best / msum if best > 0.0 else 0.0
    f0n = best_f0 / F0_MAX_HZ

    return np.asarray(
        [hi, mid, centroid, flatness, rolloff, crest, harmonicity, f0n], dtype=np.float32
    )


def frame_scalars_batch(mag: np.ndarray) -> np.ndarray:
    """frame_scalars() for every row of a (F, 513) matrix at once.

    Same arithmetic, vectorised; tests/test_features.py holds the two equal. The
    per-frame version is the readable specification, this is what the cache
    builder runs over a million frames.
    """
    mag = np.asarray(mag, dtype=np.float64)
    if mag.ndim != 2 or mag.shape[1] != N_BINS:
        raise ValueError(f"expected (frames, {N_BINS}), got {mag.shape}")
    nf = mag.shape[0]
    if nf == 0:
        return np.empty((0, N_SCALARS), dtype=np.float32)
    m = mag[:, 1:N_BINS]
    p = m * m
    total = p.sum(axis=1) + POWER_EPS

    hi = p[:, BIN_4K - 1 :].sum(axis=1) / total
    mid = p[:, BIN_1K - 1 : BIN_4K - 1].sum(axis=1) / total
    k = np.arange(1, N_BINS, dtype=np.float64)
    centroid = (p * k).sum(axis=1) / total * BIN_HZ / (SAMPLE_RATE_HZ / 2)
    flatness = np.exp(np.mean(np.log(p + POWER_EPS), axis=1)) / (p.mean(axis=1) + POWER_EPS)
    cum = np.cumsum(p, axis=1)
    target = ROLLOFF_FRACTION * cum[:, -1:]
    roll_idx = (cum < target).sum(axis=1) + 1  # == searchsorted(cum, target) + 1
    rolloff = roll_idx / (N_BINS - 1)
    msum = m.sum(axis=1) + POWER_EPS
    crest = m.max(axis=1) / (msum / (N_BINS - 1))

    cands = f0_candidates()
    combs = np.zeros((nf, cands.shape[0]), dtype=np.float64)
    for ci, f0 in enumerate(cands):
        h = np.arange(1, N_HARMONICS + 1) * f0
        h = h[h <= SAMPLE_RATE_HZ / 2]
        pos = h / BIN_HZ
        kk = np.clip(np.floor(pos).astype(np.int64), 0, N_BINS - 2)
        frac = pos - np.floor(pos)
        combs[:, ci] = (mag[:, kk] * (1.0 - frac) + mag[:, kk + 1] * frac).sum(axis=1)
    best_i = np.argmax(combs, axis=1)
    best = combs[np.arange(nf), best_i]
    harmonicity = np.where(best > 0.0, best / msum, 0.0)
    f0n = np.where(best > 0.0, cands[best_i] / F0_MAX_HZ, cands[0] / F0_MAX_HZ)

    return np.stack([hi, mid, centroid, flatness, rolloff, crest, harmonicity, f0n], axis=1).astype(
        np.float32
    )


def scalar_stats(scal: np.ndarray, logmel_win: np.ndarray) -> np.ndarray:
    """The 17 layout-2 additions from a window's (F, 8) scalars and (F, 20) log-mel."""
    scal = np.asarray(scal, dtype=np.float32)
    smean = scal.mean(axis=0, dtype=np.float32)
    sstd = np.sqrt(((scal - smean) ** 2).mean(axis=0, dtype=np.float32))
    lm = np.asarray(logmel_win, dtype=np.float32)
    if lm.shape[0] > 1:
        flux = np.float32(np.abs(np.diff(lm, axis=0)).mean(dtype=np.float32))
    else:
        flux = np.float32(0.0)
    return np.concatenate([smean, sstd, [flux]]).astype(np.float32)


def stats_spectral(mfcc_win: np.ndarray, mag_win: np.ndarray, logmel_win: np.ndarray) -> np.ndarray:
    """Layout 2 from a window's MFCC block, magnitude spectra and log-mel rows."""
    base = stats52(mfcc_win)
    scal = frame_scalars_batch(mag_win)
    return np.concatenate([base, scalar_stats(scal, logmel_win)]).astype(np.float32)


def stats_spectral_from_scalars(
    mfcc_win: np.ndarray, scal_win: np.ndarray, logmel_win: np.ndarray
) -> np.ndarray:
    """Layout 2 when the per-frame scalars are already known (the frame cache)."""
    return np.concatenate([stats52(mfcc_win), scalar_stats(scal_win, logmel_win)]).astype(
        np.float32
    )


def logmel_patch(logmel_win: np.ndarray) -> np.ndarray:
    """Layout 3: the (14, 20) log-mel block minus its scalar mean, flattened frame-major."""
    lm = np.asarray(logmel_win, dtype=np.float32)
    if lm.shape != (ACCUM_FRAMES, N_MELS):
        raise ValueError(f"expected ({ACCUM_FRAMES}, {N_MELS}), got {lm.shape}")
    return (lm - lm.mean(dtype=np.float32)).reshape(-1).astype(np.float32)


@dataclass(frozen=True)
class Layout:
    id: int
    name: str
    n_features: int


LAYOUTS = {
    LAYOUT_STATS: Layout(LAYOUT_STATS, "stats", N_STATS),
    LAYOUT_STATS_SPECTRAL: Layout(LAYOUT_STATS_SPECTRAL, "stats_spectral", N_STATS_SPECTRAL),
    LAYOUT_LOGMEL: Layout(LAYOUT_LOGMEL, "logmel", N_LOGMEL_PATCH),
}


def extract(layout: int, frames, idx: np.ndarray) -> np.ndarray:
    """Feature vector of layout `layout` for the frames `idx` of a FrameData."""
    if layout == LAYOUT_STATS:
        return stats52(frames.mfcc[idx])
    if layout == LAYOUT_STATS_SPECTRAL:
        return stats_spectral(frames.mfcc[idx], frames.mag[idx], frames.logmel[idx])
    if layout == LAYOUT_LOGMEL:
        return logmel_patch(frames.logmel[idx])
    raise ValueError(f"unknown layout {layout}")


def window_time_s(end_frame: int) -> float:
    """When a window closed, in seconds, as the board stamps DET lines."""
    return (end_frame * HOP) / SAMPLE_RATE_HZ

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

from boomdetect_train.dsp.mfcc import (
    ENV_PER_FRAME,
    ENV_RATE_HZ,
    HOP,
    N_MELS,
    N_MFCC,
    SAMPLE_RATE_HZ,
    WINDOW_SIZE,
)
from boomdetect_train.dsp.spectro import SPEC_BY_NAME, SPEC_FE_NAMES, SPEC_N_BANDS
from boomdetect_train.dsp.windows import ACCUM_FRAMES

LAYOUT_STATS = 1
LAYOUT_STATS_SPECTRAL = 2
LAYOUT_LOGMEL = 3
LAYOUT_STATS_SPECTRAL_MOD = 4
# Offline only: the modulation spectrum over a 4 s ring instead of 2 s - more segments averaged,
# so a hovering drone's steady blade-pass line rises further over the background.
# 7 = layout 2 + the ten modulation numbers over 4 s; 8 = layout 2 + both the 2 s and the 4 s sets.
LAYOUT_STATS_SPECTRAL_MOD4S = 7
LAYOUT_STATS_SPECTRAL_MOD2S4S = 8
MOD_FRAMES_LONG = 125  # 125 x 32 = 4000 samples, 4.0 s
# 9 = layout 2 + the 2 s set + the same over an 8 s ring (more averaging still);
# 10 = layout 4 + the modulation prominence spectrum itself, every bin from 10 to
# 500 Hz (dB / 20): the model sees the lines, not ten numbers about them.
LAYOUT_STATS_SPECTRAL_MOD2S8S = 9
LAYOUT_STATS_SPECTRAL_MODSPEC = 10
MOD_FRAMES_8S = 250  # 250 x 32 = 8000 samples, 8.0 s

N_STATS = 4 * N_MFCC  # 52
N_SCALARS = 8
N_STATS_SPECTRAL = N_STATS + 2 * N_SCALARS + 1  # 69
N_LOGMEL_PATCH = ACCUM_FRAMES * N_MELS  # 280
N_MOD = 10
N_STATS_SPECTRAL_MOD = N_STATS_SPECTRAL + N_MOD  # 79

# Layout 4, "stats_spectral_mod" (79): layout 2, then ten numbers from the
# modulation spectrum of the 1-4 kHz envelope over the two seconds of frames
# that end with the window (MOD_FRAMES x ENV_PER_FRAME samples at 1 kHz). A
# drone's rotor noise is amplitude-modulated at its blade-pass rate and the
# modulation survives at distances where the spectrum itself is at the
# background. Prominence of a modulation bin = its log power over the mean log
# power of the +-40 Hz around it. A window without the full ring behind it (the
# first two seconds of a clip) carries NaN here: training fills those with the
# column mean (train._impute_missing) so that no model can learn which sources
# have short clips, and the board, whose ring is always full, never produces them.
MOD_FRAMES = 62  # 62 x 32 = 1984 samples, 1.98 s
MOD_SEG = 512  # Welch segments of the envelope: 0.512 s, bins of 1.95 Hz
MOD_HOP = 128  # 75 % overlap: 12 segments over the full buffer, averaged
MOD_NFFT = MOD_SEG
MOD_BIN_HZ = ENV_RATE_HZ / MOD_NFFT  # 1.953
MOD_BASELINE_BINS = 41  # +-39 Hz
MOD_LINE_DB = 6.0
MOD_MAIN_HZ = (50.0, 400.0)
MOD_BANDS_HZ = ((50.0, 150.0), (150.0, 250.0), (250.0, 400.0), (400.0, 800.0))
MOD_SHARE_HZ = (100.0, 250.0)
MOD_TOTAL_HZ = (10.0, 500.0)
MOD_SPEC_HZ = (10.0, 500.0)
N_MOD_SPEC = int(
    (
        (np.arange(MOD_NFFT // 2 + 1) * MOD_BIN_HZ >= MOD_SPEC_HZ[0])
        & (np.arange(MOD_NFFT // 2 + 1) * MOD_BIN_HZ < MOD_SPEC_HZ[1])
    ).sum()
)  # 250 bins
MOD_NAMES = (
    "mod_prom_max",  # strongest line in 50-400 Hz, dB / 20
    "mod_lines",  # share of 50-400 Hz bins above MOD_LINE_DB
    "mod_f_peak",  # its frequency / 400 Hz
    "mod_prom_2f",  # prominence at twice that frequency, dB / 20
    "mod_band_50_150",  # strongest line per band, dB / 20
    "mod_band_150_250",
    "mod_band_250_400",
    "mod_band_400_800",
    "mod_depth",  # envelope std over mean
    "mod_share_100_250",  # envelope power 100-250 Hz over 10-500 Hz
)

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


_MOD_FREQS = np.arange(MOD_NFFT // 2 + 1) * MOD_BIN_HZ
_MOD_WINDOW = np.hanning(MOD_SEG)


def _band(lo: float, hi: float) -> np.ndarray:
    return (_MOD_FREQS >= lo) & (_MOD_FREQS < hi)


def modulation_spectrum(env: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """(power, prominence_db) of an envelope buffer of MOD_SEG..MOD_FRAMES * ENV_PER_FRAME samples.

    Welch's estimate: MOD_SEG-sample segments every MOD_HOP samples, each
    centred and Hann-windowed, their power spectra averaged - the averaging is
    what keeps a background's strongest bin near the baseline while a real
    modulation line adds up. The prominence of a bin is its log power over the
    mean log power of the MOD_BASELINE_BINS around it (a mean rather than a
    median so the C can do it with one running sum).
    """
    e = np.asarray(env, dtype=np.float64)
    n_seg = (e.shape[0] - MOD_SEG) // MOD_HOP + 1
    p = np.zeros(MOD_NFFT // 2 + 1, dtype=np.float64)
    for s in range(n_seg):
        seg = e[s * MOD_HOP : s * MOD_HOP + MOD_SEG]
        seg = (seg - seg.mean()) * _MOD_WINDOW
        p += np.abs(np.fft.rfft(seg)) ** 2
    p = p / n_seg + 1e-20
    logp = 10.0 * np.log10(p)
    half = MOD_BASELINE_BINS // 2
    padded = np.pad(logp, half, mode="edge")
    csum = np.concatenate([[0.0], np.cumsum(padded)])
    base = (csum[MOD_BASELINE_BINS:] - csum[:-MOD_BASELINE_BINS]) / MOD_BASELINE_BINS
    return p, logp - base


def modulation_stats(env_rows: np.ndarray, idx: np.ndarray, frames: int = MOD_FRAMES) -> np.ndarray:
    """The N_MOD layout-4 additions for the window whose frames are `idx`.

    Reads the envelope of the MOD_FRAMES frames that end with the window's last
    frame - the ring the board keeps. NaN for every feature until that ring is
    full (the first two seconds of a clip), see train._impute_missing.
    """
    end = int(np.asarray(idx)[-1])
    start = max(0, end - frames + 1)
    e = np.asarray(env_rows[start : end + 1], dtype=np.float32).reshape(-1)
    if e.shape[0] < frames * ENV_PER_FRAME:
        return np.full(N_MOD, np.nan, dtype=np.float32)
    out = np.zeros(N_MOD, dtype=np.float32)
    p, prom = modulation_spectrum(e)
    main = _band(*MOD_MAIN_HZ)
    k = int(np.argmax(np.where(main, prom, -np.inf)))
    f_peak = _MOD_FREQS[k]
    out[0] = prom[k] / 20.0
    out[1] = float((prom[main] > MOD_LINE_DB).mean())
    out[2] = f_peak / MOD_MAIN_HZ[1]
    k2 = int(round(2.0 * f_peak / MOD_BIN_HZ))
    lo2, hi2 = max(0, k2 - 2), min(prom.shape[0], k2 + 3)
    out[3] = (prom[lo2:hi2].max() / 20.0) if hi2 > lo2 else 0.0
    for j, (lo, hi) in enumerate(MOD_BANDS_HZ):
        out[4 + j] = prom[_band(lo, hi)].max() / 20.0
    mean = float(e.mean())
    out[8] = float(e.std()) / (mean + 1e-9) if mean > 0 else 0.0
    total = float(p[_band(*MOD_TOTAL_HZ)].sum())
    out[9] = float(p[_band(*MOD_SHARE_HZ)].sum()) / total if total > 0 else 0.0
    return out.astype(np.float32)


def modulation_prominence(
    env_rows: np.ndarray, idx: np.ndarray, frames: int = MOD_FRAMES
) -> np.ndarray:
    """Layout 10's block: the prominence (dB / 20) of every modulation bin in MOD_SPEC_HZ.

    The same ring and Welch estimate as modulation_stats; NaN until the ring is full.
    """
    end = int(np.asarray(idx)[-1])
    start = max(0, end - frames + 1)
    e = np.asarray(env_rows[start : end + 1], dtype=np.float32).reshape(-1)
    if e.shape[0] < frames * ENV_PER_FRAME:
        return np.full(N_MOD_SPEC, np.nan, dtype=np.float32)
    _, prom = modulation_spectrum(e)
    return (prom[_band(*MOD_SPEC_HZ)] / 20.0).astype(np.float32)


@dataclass(frozen=True)
class Layout:
    id: int
    name: str
    n_features: int


LAYOUTS = {
    LAYOUT_STATS: Layout(LAYOUT_STATS, "stats", N_STATS),
    LAYOUT_STATS_SPECTRAL: Layout(LAYOUT_STATS_SPECTRAL, "stats_spectral", N_STATS_SPECTRAL),
    LAYOUT_LOGMEL: Layout(LAYOUT_LOGMEL, "logmel", N_LOGMEL_PATCH),
    LAYOUT_STATS_SPECTRAL_MOD: Layout(
        LAYOUT_STATS_SPECTRAL_MOD, "stats_spectral_mod", N_STATS_SPECTRAL_MOD
    ),
    LAYOUT_STATS_SPECTRAL_MOD4S: Layout(
        LAYOUT_STATS_SPECTRAL_MOD4S, "stats_spectral_mod4s", N_STATS_SPECTRAL_MOD
    ),
    LAYOUT_STATS_SPECTRAL_MOD2S4S: Layout(
        LAYOUT_STATS_SPECTRAL_MOD2S4S, "stats_spectral_mod2s4s", N_STATS_SPECTRAL_MOD + N_MOD
    ),
    LAYOUT_STATS_SPECTRAL_MOD2S8S: Layout(
        LAYOUT_STATS_SPECTRAL_MOD2S8S, "stats_spectral_mod2s8s", N_STATS_SPECTRAL_MOD + N_MOD
    ),
    LAYOUT_STATS_SPECTRAL_MODSPEC: Layout(
        LAYOUT_STATS_SPECTRAL_MODSPEC, "stats_spectral_modspec", N_STATS_SPECTRAL_MOD + N_MOD_SPEC
    ),
}


# --- band-spectrogram layouts (offline only) ------------------------------------
#
# Layout id = base + k, k the index of the front-end in dsp.spectro.SPEC_FE_NAMES
# (mfe1k mfe2k mfe4k gs1k gs2k gs4k). None of them has a C extractor.
#
#   100 + k  spec patch      T x 64 log band power minus its mean, T = 14 (the window)
#   400 + k  spec patch long the same over T = 31 frames (~1 s) ending with the window
#   200 + k  band stats      per band: mean relative to the window mean, std, mean
#                            absolute frame-to-frame change - 192 numbers ("GS instead of
#                            layout 4" for the tree models)
#   300 + k  layout 4 + band stats (79 + 192: "GS added to layout 4")
#   500 + k  hybrid          spec patch (T = 14), then the 79 of layout 4: a network reads
#                            the patch with convolutions and layout 4 beside it
#   600 + k  hybrid long     the same with T = 31
#
# Like layout 3 every patch has its mean removed (level invariance); the band
# means of the stats are relative for the same reason. A long patch reaches back
# past the window into contiguous history, the way the modulation ring does; a
# clip too short for it (the public 0.5 s and 1 s clips) is mirrored in time.

SPEC_LONG_FRAMES = 31
N_BAND_STATS = 3 * SPEC_N_BANDS  # 192
SPEC_KINDS = {
    100: ("patch", ACCUM_FRAMES),
    200: ("bstat", ACCUM_FRAMES),
    300: ("modbstat", ACCUM_FRAMES),
    400: ("patch", SPEC_LONG_FRAMES),
    500: ("hybrid", ACCUM_FRAMES),
    600: ("hybrid", SPEC_LONG_FRAMES),
    # the network's aux input is layout 8 (2 s + 4 s modulation) or 10 (the modulation
    # prominence spectrum) instead of layout 4
    700: ("hybrid8", ACCUM_FRAMES),
    800: ("hybrid10", ACCUM_FRAMES),
    900: ("hybrid8", SPEC_LONG_FRAMES),  # the 1 s patch with the layout-8 aux: both winners in one
}
# what a hybrid kind reads beside the patch
HYBRID_AUX_LAYOUT = {
    "hybrid": LAYOUT_STATS_SPECTRAL_MOD,
    "hybrid8": LAYOUT_STATS_SPECTRAL_MOD2S4S,
    "hybrid10": LAYOUT_STATS_SPECTRAL_MODSPEC,
}


def spec_layout(layout: int) -> tuple[str, str, int] | None:
    """(kind, front-end name, patch frames) of a band-spectrogram layout, else None."""
    base, k = (layout // 100) * 100, layout % 100
    if base not in SPEC_KINDS or k >= len(SPEC_FE_NAMES):
        return None
    kind, t = SPEC_KINDS[base]
    return kind, SPEC_FE_NAMES[k], t


def min_start_frame(layout: int) -> int:
    """First frame a window of `layout` may start at so that everything it reads is real audio.

    A frame of an N-sample FFT reaches (N - 1024) / 512 frames back, a 31-frame patch
    17 frames before the window. The board always has that history; a clip may not,
    and the public sets make it a class fingerprint: 99 % of the HuggingFace drone
    clips are 0.5 s (one window, all of it at the clip start) while its negatives
    are long, so mirrored history would read as "drone". Windows without real history
    are therefore dropped, in training and in scoring alike.
    """
    spec = spec_layout(layout)
    if spec is None:
        return 0
    kind, fe, t = spec
    hist = (SPEC_BY_NAME[fe].n_fft - WINDOW_SIZE) // HOP
    return hist + (t - ACCUM_FRAMES if kind == "patch" or kind in HYBRID_AUX_LAYOUT else 0)


def spec_layout_id(kind_base: int, fe_name: str) -> int:
    return kind_base + SPEC_FE_NAMES.index(fe_name)


def _spec_width(kind: str, t: int) -> int:
    if kind == "patch":
        return t * SPEC_N_BANDS
    if kind == "bstat":
        return N_BAND_STATS
    if kind == "modbstat":
        return N_STATS_SPECTRAL_MOD + N_BAND_STATS
    return t * SPEC_N_BANDS + LAYOUTS[HYBRID_AUX_LAYOUT[kind]].n_features  # hybrid*


for _base, (_kind, _t) in SPEC_KINDS.items():
    for _k, _fe in enumerate(SPEC_FE_NAMES):
        _id = _base + _k
        LAYOUTS[_id] = Layout(
            _id, f"{_kind}{'' if _t == ACCUM_FRAMES else _t}_{_fe}", _spec_width(_kind, _t)
        )


def spec_patch(spec_rows: np.ndarray, idx: np.ndarray, t: int = ACCUM_FRAMES) -> np.ndarray:
    """T x 64 patch minus its scalar mean, frame-major: the window's frames, or the
    T contiguous frames ending with its last one when T is longer than the window."""
    idx = np.asarray(idx)
    if t == idx.shape[0]:
        s = np.asarray(spec_rows[idx], dtype=np.float32)
    else:
        end = int(idx[-1])
        start = end - t + 1
        if start >= 0:
            s = np.asarray(spec_rows[start : end + 1], dtype=np.float32)
        else:
            s = np.pad(
                np.asarray(spec_rows[: end + 1], dtype=np.float32),
                ((-start, 0), (0, 0)),
                "symmetric",
            )
    return (s - s.mean(dtype=np.float32)).reshape(-1).astype(np.float32)


def band_stats(spec_win: np.ndarray) -> np.ndarray:
    """Layout 2xx: per band mean (minus the window mean), std, mean |delta|; band-major blocks."""
    s = np.asarray(spec_win, dtype=np.float32)
    m = s.mean(axis=0)
    d = np.abs(np.diff(s, axis=0)).mean(axis=0) if s.shape[0] > 1 else np.zeros_like(m)
    return np.concatenate([m - m.mean(), s.std(axis=0), d]).astype(np.float32)


def extract(layout: int, frames, idx: np.ndarray) -> np.ndarray:
    """Feature vector of layout `layout` for the frames `idx` of a FrameData."""
    if layout == LAYOUT_STATS:
        return stats52(frames.mfcc[idx])
    if layout == LAYOUT_STATS_SPECTRAL:
        return stats_spectral(frames.mfcc[idx], frames.mag[idx], frames.logmel[idx])
    if layout == LAYOUT_LOGMEL:
        return logmel_patch(frames.logmel[idx])
    if layout == LAYOUT_STATS_SPECTRAL_MOD:
        base = stats_spectral(frames.mfcc[idx], frames.mag[idx], frames.logmel[idx])
        return np.concatenate([base, modulation_stats(frames.env, idx)]).astype(np.float32)
    if spec_layout(layout) is not None:
        raise ValueError(
            f"layout {layout} reads the band-spectrogram cache: evaluate.window_features"
        )
    raise ValueError(f"unknown layout {layout}")

"""Frame-level front end: framing, RMS, magnitude spectrum, log-mel, MFCC.

Mirrors src/boomdetect.c (framing and RMS) and src/boomdetect_mfcc_f32.c (the
transform). The one deliberate difference from the board is that the RMS
pre-conditioning is not reproduced: the C divides the frame by its RMS before
the FFT and multiplies the magnitudes back afterwards, which is the identity in
exact arithmetic and only exists to keep float32 well scaled. Here the FFT runs
in float64, so the identity holds and the parity test measures the residual.

Frames are BOOMDETECT_WINDOW_SIZE (1024) samples at 16 kHz with a hop of 512,
starting at sample 0 with no padding - the board takes them out of a FIFO the
same way, so frame f covers samples [512 f, 512 f + 1024).
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np

from boomdetect_train.dsp.tables import MfccTables, load_tables

SAMPLE_RATE_HZ = 16000
WINDOW_SIZE = 1024
HOP = 512
N_MFCC = 13
N_MELS = 20
# Added before the log on the board (arm_offset_f32 by 1.0e-6f).
LOG_OFFSET = 1.0e-6


def n_frames(n_samples: int, window: int = WINDOW_SIZE, hop: int = HOP) -> int:
    """How many frames the board would take out of `n_samples` decimated samples."""
    if n_samples < window:
        return 0
    return 1 + (n_samples - window) // hop


def frame_signal(x: np.ndarray, window: int = WINDOW_SIZE, hop: int = HOP) -> np.ndarray:
    """(n_frames, window) view of `x`, frame f starting at sample hop * f."""
    x = np.ascontiguousarray(x, dtype=np.float32)
    n = n_frames(x.shape[0], window, hop)
    if n == 0:
        return np.empty((0, window), dtype=np.float32)
    return np.lib.stride_tricks.sliding_window_view(x, window)[::hop][:n]


def frame_rms(frames: np.ndarray) -> np.ndarray:
    """Per-frame RMS, full scale = 1.0 (arm_rms_f32 on the un-windowed frame)."""
    if frames.shape[0] == 0:
        return np.empty((0,), dtype=np.float32)
    return np.sqrt(np.mean(frames.astype(np.float64) ** 2, axis=1)).astype(np.float32)


@dataclass
class FrameData:
    """Everything the board knows about each frame before aggregation."""

    rms: np.ndarray  # (F,) float32
    mag: np.ndarray  # (F, n_bins) float64 magnitude spectrum, bins 0..N/2
    logmel: np.ndarray  # (F, n_mels) float32, ln(mel + 1e-6)
    mfcc: np.ndarray  # (F, n_mfcc) float32
    env: np.ndarray  # (F, ENV_PER_FRAME) float32, the 1-4 kHz envelope of each hop at 1 kHz

    @property
    def n(self) -> int:
        return int(self.rms.shape[0])


# The envelope behind the modulation features (features.modulation_stats): the
# 1-4 kHz band - where the DJI's rotor noise sits - rectified, low-passed and
# kept at 1 kHz, 32 samples per 512-sample hop. A hovering drone's noise is
# amplitude-modulated at its blade-pass rate (~170-180 Hz for a Phantom 4) and
# the modulation stays put while the rotor lines themselves wander; backgrounds
# have none. Causal biquads only, so the C can run the same chain sample by sample.
ENV_RATE_HZ = 1000
ENV_DECIM = SAMPLE_RATE_HZ // ENV_RATE_HZ  # 16
ENV_PER_FRAME = HOP // ENV_DECIM  # 32
ENV_BAND_HZ = (1000.0, 4000.0)
ENV_LOWPASS_HZ = 400.0


_ENV_SOS: tuple[np.ndarray, np.ndarray] | None = None


def envelope_filters() -> tuple[np.ndarray, np.ndarray]:
    """(band-pass, low-pass) second-order sections, designed once; the C gets the coefficients."""
    global _ENV_SOS
    if _ENV_SOS is None:
        from scipy import signal

        bp = signal.butter(2, ENV_BAND_HZ, btype="bandpass", fs=SAMPLE_RATE_HZ, output="sos")
        lp = signal.butter(2, ENV_LOWPASS_HZ, btype="lowpass", fs=SAMPLE_RATE_HZ, output="sos")
        _ENV_SOS = (bp, lp)
    return _ENV_SOS


def envelope_1k(x16k: np.ndarray) -> np.ndarray:
    """The 1-4 kHz envelope of a 16 kHz signal, sampled at 1 kHz (len(x) // 16 values)."""
    from scipy import signal

    bp, lp = envelope_filters()
    y = signal.sosfilt(bp, np.asarray(x16k, dtype=np.float64))
    e = signal.sosfilt(lp, np.abs(y))
    return e[::ENV_DECIM].astype(np.float32)


def frame_envelope(env: np.ndarray, n_frames_: int) -> np.ndarray:
    """(F, ENV_PER_FRAME): the envelope samples of each frame's hop, frame f at hop f."""
    need = n_frames_ * ENV_PER_FRAME
    if n_frames_ == 0:
        return np.empty((0, ENV_PER_FRAME), dtype=np.float32)
    if env.shape[0] < need:
        env = np.concatenate([env, np.zeros(need - env.shape[0], dtype=np.float32)])
    return env[:need].reshape(n_frames_, ENV_PER_FRAME)


class Frontend:
    """The per-frame chain, bound to one set of tables."""

    def __init__(self, tables: MfccTables | None = None):
        self.tables = tables if tables is not None else load_tables()
        self._mel = self.tables.mel_matrix()  # (n_mels, n_bins)
        self._dct = self.tables.dct.astype(np.float64)  # (n_mfcc, n_mels)
        self._window = self.tables.window.astype(np.float64)

    def spectrum(self, frames: np.ndarray) -> np.ndarray:
        """Magnitude spectrum of Hamming-windowed frames, (F, n_bins) float64.

        No scaling: arm_rfft_fast_f32 forward is unscaled, and so is numpy's.
        """
        if frames.shape[0] == 0:
            return np.empty((0, self.tables.n_bins), dtype=np.float64)
        spec = np.fft.rfft(frames.astype(np.float64) * self._window, axis=1)
        return np.abs(spec)

    def logmel_from_mag(self, mag: np.ndarray) -> np.ndarray:
        mel = mag @ self._mel.T
        return np.log(mel + LOG_OFFSET).astype(np.float32)

    def mfcc_from_logmel(self, logmel: np.ndarray) -> np.ndarray:
        return (logmel.astype(np.float64) @ self._dct.T).astype(np.float32)

    def process(self, x16k: np.ndarray) -> FrameData:
        """Run the whole per-frame chain over a 16 kHz float signal."""
        frames = frame_signal(x16k)
        rms = frame_rms(frames)
        mag = self.spectrum(frames)
        logmel = self.logmel_from_mag(mag)
        mfcc = self.mfcc_from_logmel(logmel)
        env = frame_envelope(envelope_1k(x16k), frames.shape[0])
        return FrameData(rms=rms, mag=mag, logmel=logmel, mfcc=mfcc, env=env)

"""Loading audio into the shape the chain wants: float32, mono, 16 kHz.

Two paths to 16 kHz, and they are not the same thing:

* 48 kHz input is decimated by taking every third sample, exactly as
  boomdetect_push() does. Valid only because the node's PDM chain already
  band-limits to 8 kHz; on a 48 kHz WAV from elsewhere it aliases, and the
  caller has to know which it has.
* Anything else is resampled with a polyphase filter. That is what the public
  datasets get, and it is a training-time approximation of a microphone chain
  the model will never see at run time.

int16 PCM is scaled by 1/32768, matching the board.
"""

from __future__ import annotations

import io
from math import gcd
from pathlib import Path

import numpy as np
import soundfile as sf
from scipy.signal import resample_poly

from boomdetect_train.dsp.mfcc import SAMPLE_RATE_HZ

PCM_SCALE = 1.0 / 32768.0


def to_mono(x: np.ndarray) -> np.ndarray:
    if x.ndim == 1:
        return x
    return x.mean(axis=1)


def decimate3(x48k: np.ndarray, phase: int = 0) -> np.ndarray:
    """The firmware's 48 kHz -> 16 kHz path: every third sample, no filter."""
    return np.ascontiguousarray(x48k[phase::3])


def to_16k(x: np.ndarray, sr: int, *, firmware_decimation: bool = True) -> np.ndarray:
    """Bring `x` (mono float) to 16 kHz."""
    x = np.asarray(x, dtype=np.float32)
    if sr == SAMPLE_RATE_HZ:
        return x
    if sr == 3 * SAMPLE_RATE_HZ and firmware_decimation:
        return decimate3(x).astype(np.float32)
    g = gcd(sr, SAMPLE_RATE_HZ)
    return resample_poly(x.astype(np.float64), SAMPLE_RATE_HZ // g, sr // g).astype(np.float32)


def pcm16_to_float(pcm: np.ndarray) -> np.ndarray:
    return pcm.astype(np.float32) * np.float32(PCM_SCALE)


def float_to_pcm16(x: np.ndarray) -> np.ndarray:
    """Quantise to int16 the way a recording would arrive. Clips at full scale."""
    return np.clip(np.round(np.asarray(x, dtype=np.float64) * 32768.0), -32768, 32767).astype(
        np.int16
    )


def read_audio(source: Path | str | bytes) -> tuple[np.ndarray, int]:
    """Read a file path or an in-memory WAV/FLAC; returns (mono float32, sample rate).

    Integer PCM is read as int16 and scaled by 1/32768 so that a 16-bit file
    reproduces the board's own scaling bit for bit. Float files (the Salford
    recordings) come back as they are.
    """
    handle = io.BytesIO(source) if isinstance(source, bytes) else str(source)
    with sf.SoundFile(handle) as f:
        sr = f.samplerate
        if f.subtype in ("PCM_16", "PCM_S8", "PCM_U8"):
            data = f.read(dtype="int16", always_2d=False)
            x = pcm16_to_float(to_mono(data.astype(np.float32)))
        else:
            data = f.read(dtype="float32", always_2d=False)
            x = to_mono(data).astype(np.float32)
    return x, sr


def load_16k(source: Path | str | bytes, *, firmware_decimation: bool = True) -> np.ndarray:
    """read_audio() followed by to_16k()."""
    x, sr = read_audio(source)
    return to_16k(x, sr, firmware_decimation=firmware_decimation)

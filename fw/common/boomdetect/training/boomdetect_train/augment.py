"""Field augmentation: the same recording heard from further away.

The field recordings were made within tens of metres of the drone, and the
first models trained on them lean on the top of the band: low-pass a recording
at 6 kHz and every one of them stops detecting, the DJI included. Distance does
a milder version of that to every real drone - air absorbs 8 kHz at roughly
0.08 dB per metre and 1 kHz at 0.005 - and the negatives played from a phone
were band-limited to begin with, so "no highs" had become a cue for "not a
drone". A variant here is therefore:

* a zero-phase FIR with the air-absorption loss of a random distance (ISO
  9613-1, 20 degC, 60 % RH, interpolated in log-frequency);
* for a drone, an attenuation, so the recording sinks towards the floor;
* a synthetic floor: white noise at the level of the node's microphone
  self-noise plus a low rumble below 200 Hz, the two things every quiet field
  recording is made of. Synthetic on purpose - mixing in a recorded background
  would carry a held-out recording into the folds that train on it.

Negatives get the same absorption and floor (without the attenuation), so a
band limit stops being a property of one class. Every variant is keyed to its
recording: an augmented clip keeps the group of the clip it came from, and a
fold that holds a recording out holds its variants out with it.
"""

from __future__ import annotations

import hashlib
from dataclasses import dataclass

import numpy as np
from scipy import signal

# ISO 9613-1 pure-tone absorption, dB per metre, 20 degC, 60 % relative humidity.
AIR_DB_PER_M = {
    63: 0.0001,
    125: 0.0004,
    250: 0.0011,
    500: 0.0024,
    1000: 0.0045,
    2000: 0.0087,
    4000: 0.0240,
    8000: 0.0830,
    16000: 0.2700,
}
FIR_TAPS = 511  # at 48 kHz: 94 Hz resolution, enough for a loss that is smooth in log f


@dataclass(frozen=True)
class Variant:
    distance_m: float
    gain_db: float  # applied to the recording before the floor is added
    white_dbfs: float  # RMS of the white floor
    rumble_dbfs: float  # RMS of the < 200 Hz rumble


def air_loss_db(freqs_hz: np.ndarray, distance_m: float) -> np.ndarray:
    """Loss in dB at each frequency over `distance_m` of air (log-f interpolation)."""
    f = np.asarray(sorted(AIR_DB_PER_M), dtype=np.float64)
    a = np.asarray([AIR_DB_PER_M[k] for k in sorted(AIR_DB_PER_M)], dtype=np.float64)
    lf = np.log10(np.clip(np.asarray(freqs_hz, dtype=np.float64), f[0], f[-1]))
    return np.interp(lf, np.log10(f), a) * distance_m


def air_fir(distance_m: float, sr: int, taps: int = FIR_TAPS) -> np.ndarray:
    freqs = np.linspace(0.0, sr / 2.0, 257)
    gains = 10.0 ** (-air_loss_db(freqs, distance_m) / 20.0)
    return signal.firwin2(taps, freqs, gains, fs=sr)


def _rms_to(x: np.ndarray, dbfs: float) -> np.ndarray:
    rms = float(np.sqrt(np.mean(x.astype(np.float64) ** 2))) + 1e-12
    return x * (10.0 ** (dbfs / 20.0) / rms)


def floor_noise(n: int, sr: int, v: Variant, rng: np.random.Generator) -> np.ndarray:
    white = _rms_to(rng.standard_normal(n), v.white_dbfs)
    sos = signal.butter(2, 200.0, btype="lowpass", fs=sr, output="sos")
    rumble = _rms_to(signal.sosfilt(sos, rng.standard_normal(n)), v.rumble_dbfs)
    return white + rumble


def absorb(x: np.ndarray, sr: int, distance_m: float) -> np.ndarray:
    """`x` after `distance_m` of air. The FIR is symmetric, so centring it is zero-phase
    with the loss applied once (filtfilt would square it: twice the distance)."""
    return signal.fftconvolve(np.asarray(x, dtype=np.float64), air_fir(distance_m, sr), "same")


def apply_variant(x: np.ndarray, sr: int, v: Variant, rng: np.random.Generator) -> np.ndarray:
    """One variant of a recording: absorbed, attenuated, on a fresh floor. float32 out."""
    y = absorb(x, sr, v.distance_m)
    y = y * 10.0 ** (v.gain_db / 20.0) + floor_noise(y.shape[0], sr, v, rng)
    return np.clip(y, -1.0, 32767.0 / 32768.0).astype(np.float32)


def draw_variant(rng: np.random.Generator, *, drone: bool) -> Variant:
    """Random distance log-uniform over 20-250 m; a drone also sinks 3-20 dB."""
    return Variant(
        distance_m=float(np.exp(rng.uniform(np.log(20.0), np.log(250.0)))),
        gain_db=float(rng.uniform(-20.0, -3.0)) if drone else 0.0,
        white_dbfs=float(rng.uniform(-70.0, -62.0)),
        rumble_dbfs=float(rng.uniform(-58.0, -48.0)),
    )


def seed_for(clip_id: str, k: int, seed: int = 0) -> int:
    """A variant's seed: stable across runs, different for every clip and variant."""
    h = hashlib.sha1(f"{seed}/{clip_id}/{k}".encode()).digest()
    return int.from_bytes(h[:8], "big")


def variants_of(
    clip_id: str, x: np.ndarray, sr: int, label: int, count: int, seed: int = 0
) -> list[tuple[str, np.ndarray, Variant]]:
    """`count` variants of one clip, as (variant clip id, audio, parameters)."""
    out = []
    for k in range(count):
        rng = np.random.default_rng(seed_for(clip_id, k, seed))
        v = draw_variant(rng, drone=label == 1)
        out.append((f"{clip_id}#aug{k + 1}", apply_variant(x, sr, v, rng), v))
    return out

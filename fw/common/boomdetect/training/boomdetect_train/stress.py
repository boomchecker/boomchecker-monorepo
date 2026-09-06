"""Synthetic stress clips: the sounds that fooled the deployed model, on demand.

The mlp_v6 detector fired on a person humming with a closed mouth. Until the
node's microphone can record real hums again, these probes stand in: harmonic
series with pitch jitter and vibrato, low-passed the way a closed mouth
low-passes a voice; whistles; and two broadband controls. They are labelled 0
and carry the `stress` role, so they only ever evaluate - a model trained on
synthetic hums would learn the synthesiser, not the failure.

Everything is deterministic (seeded) and written as 16 kHz 16-bit WAV, so the
clips go through exactly the same path as any other file.
"""

from __future__ import annotations

from pathlib import Path

import numpy as np
import soundfile as sf

from boomdetect_train.dsp.audio import float_to_pcm16
from boomdetect_train.paths import raw_dir

SR = 16000
CLIP_S = 5.0
SEED = 2026


def _vibrato_phase(f0: float, depth_cents: float, rate_hz: float, n: int, rng) -> np.ndarray:
    """Instantaneous phase of a tone at f0 with vibrato and slow random pitch drift."""
    t = np.arange(n) / SR
    drift = np.cumsum(rng.normal(size=n)) / SR * 6.0  # random walk, a few Hz over the clip
    cents = depth_cents * np.sin(2 * np.pi * rate_hz * t) + drift
    inst = f0 * 2.0 ** (cents / 1200.0)
    return 2 * np.pi * np.cumsum(inst) / SR


def hum(f0: float, rng, n_harm: int = 12, decay: float = 1.3, lpf_hz: float = 3000.0) -> np.ndarray:
    n = int(CLIP_S * SR)
    phase = _vibrato_phase(f0, depth_cents=25.0, rate_hz=5.5, n=n, rng=rng)
    x = np.zeros(n)
    for h in range(1, n_harm + 1):
        f = f0 * h
        if f >= SR / 2:
            break
        # A closed mouth rolls the harmonics off steeply above a few kHz.
        att = 1.0 / (1.0 + (f / lpf_hz) ** 4)
        x += np.sin(h * phase + rng.uniform(0, 2 * np.pi)) * att / h**decay
    env = 0.5 + 0.5 * np.clip(np.sin(2 * np.pi * 0.3 * np.arange(n) / SR + 1.0), 0, 1)  # breathing
    x *= env
    x += rng.normal(size=n) * 0.002  # room floor
    return 0.15 * x / (np.max(np.abs(x)) + 1e-9)


def whistle(f0: float, rng) -> np.ndarray:
    n = int(CLIP_S * SR)
    phase = _vibrato_phase(f0, depth_cents=40.0, rate_hz=6.0, n=n, rng=rng)
    x = np.sin(phase) + 0.05 * np.sin(2 * phase)
    x += rng.normal(size=n) * 0.003
    return 0.12 * x / (np.max(np.abs(x)) + 1e-9)


def pink_noise(rng) -> np.ndarray:
    n = int(CLIP_S * SR)
    white = rng.normal(size=n)
    spec = np.fft.rfft(white)
    freqs = np.fft.rfftfreq(n, 1 / SR)
    spec[1:] /= np.sqrt(freqs[1:])
    x = np.fft.irfft(spec, n)
    return 0.1 * x / (np.max(np.abs(x)) + 1e-9)


def fan(rng, mains_hz: float = 50.0) -> np.ndarray:
    """Broadband low-passed noise plus a mains hum: a desk fan or a fridge."""
    n = int(CLIP_S * SR)
    white = rng.normal(size=n)
    spec = np.fft.rfft(white)
    freqs = np.fft.rfftfreq(n, 1 / SR)
    spec *= 1.0 / (1.0 + (freqs / 1500.0) ** 2)
    x = np.fft.irfft(spec, n)
    t = np.arange(n) / SR
    x += 0.3 * np.sin(2 * np.pi * mains_hz * t) + 0.1 * np.sin(2 * np.pi * 2 * mains_hz * t)
    return 0.1 * x / (np.max(np.abs(x)) + 1e-9)


def build_stress_clips(root: Path | None = None, seed: int = SEED) -> list[Path]:
    """Write the probe set; returns the files. Idempotent for a fixed seed."""
    root = root if root is not None else raw_dir() / "stress"
    root.mkdir(parents=True, exist_ok=True)
    rng = np.random.default_rng(seed)
    out: list[Path] = []

    def write(name: str, x: np.ndarray) -> None:
        p = root / f"{name}.wav"
        sf.write(str(p), float_to_pcm16(x), SR, subtype="PCM_16")
        out.append(p)

    for i, f0 in enumerate((90, 110, 130, 150, 175, 200, 230, 260)):
        write(f"hum_closed_{i:02d}_{f0}hz", hum(float(f0), rng))
    for i, f0 in enumerate((100, 140, 190, 240)):
        write(f"hum_open_{i:02d}_{f0}hz", hum(float(f0), rng, n_harm=20, decay=1.0, lpf_hz=6000.0))
    for i, f0 in enumerate((900, 1300, 1800, 2400)):
        write(f"whistle_{i:02d}_{f0}hz", whistle(float(f0), rng))
    for i in range(3):
        write(f"pink_{i:02d}", pink_noise(rng))
    for i, mains in enumerate((50.0, 60.0)):
        write(f"fan_{i:02d}", fan(rng, mains))
    return out


def category_of(stem: str) -> str:
    return stem.split("_")[0] + ("_" + stem.split("_")[1] if stem.startswith("hum") else "")

"""The experimental per-frame wind floor (dsp.mfcc.spectral_floor) keeps tonal lines and removes
the broadband hump under them."""

import numpy as np

from boomdetect_train.dsp.mfcc import HOP, WINDOW_SIZE, Frontend, frame_signal, spectral_floor

SR = 16000
TONES = (175.0, 350.0, 525.0)


def _signal(seconds: float = 2.0) -> np.ndarray:
    rng = np.random.default_rng(3)
    n = int(SR * seconds)
    brown = np.cumsum(rng.standard_normal(n))
    brown -= np.convolve(brown, np.ones(801) / 801, mode="same")  # drop the drift below ~20 Hz
    brown *= 0.05 / np.std(brown)
    t = np.arange(n) / SR
    tones = sum(0.01 * np.sin(2 * np.pi * f * t) for f in TONES)
    return (brown + tones).astype(np.float32)


def _bin(f: float) -> int:
    return int(round(f / (SR / WINDOW_SIZE)))


def test_tones_kept_and_floor_removed():
    fe = Frontend()
    mag = fe.spectrum(frame_signal(_signal()))
    out = spectral_floor(mag, (800.0, 4, 1.0, 0.1))
    avg_in, avg_out = mag.mean(axis=0), out.mean(axis=0)
    for f in TONES:
        k = _bin(f)
        tone_in = avg_in[k] - np.median(avg_in[k - 4 : k + 5])
        tone_out = avg_out[k]
        assert tone_out > 0.7 * tone_in, f"{f} Hz line lost: {tone_out:.3f} vs {tone_in:.3f}"
    gaps = [_bin(f) for f in (110.0, 260.0, 440.0)]
    for k in gaps:
        assert avg_out[k] < 0.32 * avg_in[k], f"floor at bin {k} not removed"
    hi = _bin(2000.0)
    assert np.allclose(out[:, hi:], mag[:, hi:]), "above top_hz the spectrum must be untouched"


def test_off_without_params():
    mag = np.abs(np.random.default_rng(1).standard_normal((5, WINDOW_SIZE // 2 + 1)))
    assert spectral_floor(mag, None) is mag
    assert HOP == WINDOW_SIZE // 2

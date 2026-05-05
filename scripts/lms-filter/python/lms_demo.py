from __future__ import annotations

import argparse
import ctypes
import json
import os
from pathlib import Path

os.environ.setdefault(
    "MPLCONFIGDIR", str(Path(__file__).resolve().parent.parent / "build" / "mplconfig")
)

import matplotlib

matplotlib.use("Agg")

import matplotlib.pyplot as plt
import numpy as np
from scipy.io.wavfile import write
from scipy.signal import butter, lfilter


ROOT_DIR = Path(__file__).resolve().parent.parent
DEFAULT_LIB_PATH = ROOT_DIR / "build" / "liblms_filter.so"
DEFAULT_OUTPUT_DIR = Path(__file__).resolve().parent / "out"


def bandpass_noise(num_samples: int, fs: int, low_hz: float, high_hz: float, gain: float) -> np.ndarray:
    noise = np.random.randn(num_samples)
    high_hz = min(high_hz, 0.95 * fs / 2)
    b, a = butter(4, [low_hz / (fs / 2), high_hz / (fs / 2)], btype="bandpass")
    return gain * lfilter(b, a, noise)


def normalize_audio(x: np.ndarray, peak: float = 0.9) -> np.ndarray:
    max_val = float(np.max(np.abs(x)))
    if max_val < 1e-12:
        return x.astype(np.float32)
    return (peak * x / max_val).astype(np.float32)


def generate_rpm_profile(t: np.ndarray, rpm_base: float, rpm_variation: float) -> np.ndarray:
    rpm = (
        rpm_base
        + rpm_variation * np.sin(2 * np.pi * 0.18 * t)
        + 0.35 * rpm_variation * np.sin(2 * np.pi * 0.047 * t + 1.3)
    )
    rpm += 2000 * np.exp(-0.5 * ((t - 3.0) / 0.15) ** 2)
    rpm += -1500 * np.exp(-0.5 * ((t - 7.0) / 0.2) ** 2)
    return rpm


def generate_drone_sound(
    fs: int,
    duration_s: float,
    rpm_base: float = 9000,
    rpm_variation: float = 1500,
    propeller_blades: int = 2,
) -> np.ndarray:
    num_samples = int(fs * duration_s)
    t = np.arange(num_samples) / fs
    rpm = generate_rpm_profile(t, rpm_base, rpm_variation)
    rotation_freq = rpm / 60.0
    blade_pass_freq = propeller_blades * rotation_freq
    phase = 2 * np.pi * np.cumsum(blade_pass_freq) / fs

    signal = np.zeros_like(t)
    for harmonic, gain in enumerate([1.0, 0.55, 0.35, 0.23, 0.16, 0.11, 0.08], start=1):
        signal += gain * np.sin(harmonic * phase + 0.2 * np.sin(2 * np.pi * 0.6 * t))

    amp_mod = 1.0 + 0.18 * np.sin(2 * np.pi * 3.2 * t)
    amp_mod += 0.08 * np.sin(2 * np.pi * 7.1 * t + 0.8)
    amp_mod += 0.3 * np.exp(-0.5 * ((t - 3.0) / 0.2) ** 2)
    amp_mod += 0.25 * np.exp(-0.5 * ((t - 7.0) / 0.25) ** 2)
    signal *= amp_mod

    turbulence = bandpass_noise(num_samples, fs, 800, 7000, gain=0.35)
    turbulence *= 1 + 0.5 * np.exp(-0.5 * ((t - 3.0) / 0.2) ** 2)
    turbulence *= 1 + 0.4 * np.exp(-0.5 * ((t - 7.0) / 0.25) ** 2)

    vibration = 0.25 * np.sin(2 * np.pi * rotation_freq.mean() * t)
    vibration += 0.12 * np.sin(2 * np.pi * 2 * rotation_freq.mean() * t)
    return normalize_audio(signal + turbulence + vibration, peak=0.75)


def apply_reference_path(reference: np.ndarray, fs: int) -> np.ndarray:
    delay_samples = max(1, int(round(0.002 * fs)))
    path = np.array([0.70, -0.24, 0.16, 0.09, -0.05, 0.03], dtype=np.float32)
    delayed = np.pad(reference, (delay_samples, 0), mode="constant")[: len(reference)]
    return lfilter(path, [1.0], delayed).astype(np.float32)


def to_q15(x: np.ndarray, peak: float = 0.85) -> np.ndarray:
    max_val = float(np.max(np.abs(x)))
    if max_val > peak:
        x = x * (peak / max_val)
    return np.clip(np.round(x * 32767.0), -32768, 32767).astype(np.int16)


def from_q15(x: np.ndarray) -> np.ndarray:
    return x.astype(np.float32) / 32768.0


def load_lms_lib(lib_path: Path) -> ctypes.CDLL:
    if not lib_path.exists():
        raise FileNotFoundError(f"LMS library not found: {lib_path}. Run `task build` first.")
    lib = ctypes.CDLL(str(lib_path))
    ptr_i16 = np.ctypeslib.ndpointer(dtype=np.int16, ndim=1, flags="C_CONTIGUOUS")
    lib.lms_filter_i16.argtypes = [ptr_i16, ptr_i16, ptr_i16, ptr_i16, ctypes.c_size_t]
    lib.lms_filter_i16.restype = ctypes.c_int
    return lib


def run_lms(lib_path: Path, reference_q15: np.ndarray, desired_q15: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    lib = load_lms_lib(lib_path)
    cleaned = np.zeros_like(desired_q15)
    estimated = np.zeros_like(desired_q15)
    status = lib.lms_filter_i16(reference_q15, desired_q15, cleaned, estimated, desired_q15.size)
    if status != 0:
        raise RuntimeError(f"lms_filter_i16 failed with status {status}")
    return cleaned, estimated


def mse(a: np.ndarray, b: np.ndarray) -> float:
    err = a.astype(np.float64) - b.astype(np.float64)
    return float(np.mean(err * err))


def snr_db(clean: np.ndarray, observed: np.ndarray) -> float:
    signal_power = float(np.mean(clean.astype(np.float64) ** 2))
    noise_power = mse(clean, observed)
    return 10.0 * np.log10(signal_power / max(noise_power, 1e-20))


def plot_results(
    out_path: Path,
    fs: int,
    clean: np.ndarray,
    noisy: np.ndarray,
    cleaned: np.ndarray,
    error: np.ndarray,
    seconds: float = 0.15,
) -> None:
    count = min(len(clean), int(seconds * fs))
    t = np.arange(count) / fs
    fig, axes = plt.subplots(4, 1, figsize=(12, 8), sharex=True)
    axes[0].plot(t, clean[:count])
    axes[0].set_title("Clean input")
    axes[1].plot(t, noisy[:count])
    axes[1].set_title("Noisy input")
    axes[2].plot(t, cleaned[:count])
    axes[2].set_title("LMS cleaned output")
    axes[3].plot(t, error[:count])
    axes[3].set_title("Error vs clean")
    axes[3].set_xlabel("Time [s]")
    for ax in axes:
        ax.grid(True, alpha=0.25)
        ax.set_ylim(-1.05, 1.05)
    fig.tight_layout()
    fig.savefig(out_path, dpi=140)
    plt.close(fig)


def build_demo(fs: int, duration_s: float, sine_hz: float, seed: int) -> dict[str, np.ndarray]:
    np.random.seed(seed)
    samples = int(fs * duration_s)
    t = np.arange(samples) / fs
    clean = 0.45 * np.sin(2 * np.pi * sine_hz * t)
    reference = generate_drone_sound(fs=fs, duration_s=duration_s)
    disturbance = 0.55 * apply_reference_path(reference, fs)
    noisy = clean + disturbance
    peak = float(np.max(np.abs(noisy)))
    if peak > 0.95:
        scale = 0.95 / peak
        clean = clean * scale
        disturbance = disturbance * scale
        noisy = noisy * scale
    return {
        "clean": clean.astype(np.float32),
        "reference": reference.astype(np.float32),
        "disturbance": disturbance.astype(np.float32),
        "noisy": noisy.astype(np.float32),
    }


def run_demo(
    lib_path: Path = DEFAULT_LIB_PATH,
    output_dir: Path = DEFAULT_OUTPUT_DIR,
    fs: int = 16000,
    duration_s: float = 8.0,
    sine_hz: float = 440.0,
    seed: int = 7,
    reference_gain: float = 1.0,
    save_wav: bool = True,
) -> dict[str, float | str]:
    output_dir.mkdir(parents=True, exist_ok=True)
    data = build_demo(fs=fs, duration_s=duration_s, sine_hz=sine_hz, seed=seed)
    reference_q15 = to_q15(data["reference"] * reference_gain, peak=0.9)
    noisy_q15 = to_q15(data["noisy"], peak=0.9)
    cleaned_q15, estimated_q15 = run_lms(lib_path, reference_q15, noisy_q15)

    clean = data["clean"]
    noisy = from_q15(noisy_q15)
    cleaned = from_q15(cleaned_q15)
    error = cleaned - clean

    metrics = {
        "mse_noisy": mse(clean, noisy),
        "mse_cleaned": mse(clean, cleaned),
        "snr_noisy_db": snr_db(clean, noisy),
        "snr_cleaned_db": snr_db(clean, cleaned),
    }
    metrics["snr_improvement_db"] = metrics["snr_cleaned_db"] - metrics["snr_noisy_db"]

    plot_path = output_dir / "lms_demo.png"
    plot_results(plot_path, fs, clean, noisy, cleaned, error)

    metrics_path = output_dir / "metrics.json"
    metrics_with_paths: dict[str, float | str] = dict(metrics)
    metrics_with_paths["plot_path"] = str(plot_path)
    metrics_with_paths["reference_gain"] = reference_gain
    metrics_path.write_text(json.dumps(metrics_with_paths, indent=2), encoding="utf-8")

    if save_wav:
        write(output_dir / "clean.wav", fs, to_q15(clean))
        write(output_dir / "noisy.wav", fs, noisy_q15)
        write(output_dir / "cleaned.wav", fs, cleaned_q15)
        write(output_dir / "estimated_noise.wav", fs, estimated_q15)

    return metrics_with_paths


def main() -> None:
    parser = argparse.ArgumentParser(description="Run synthetic Q15 LMS drone-noise demo.")
    parser.add_argument("--lib-path", type=Path, default=DEFAULT_LIB_PATH)
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT_DIR)
    parser.add_argument("--fs", type=int, default=16000)
    parser.add_argument("--duration-s", type=float, default=8.0)
    parser.add_argument("--sine-hz", type=float, default=440.0)
    parser.add_argument("--seed", type=int, default=7)
    parser.add_argument("--reference-gain", type=float, default=1.0)
    parser.add_argument("--no-wav", action="store_true")
    args = parser.parse_args()

    metrics = run_demo(
        lib_path=args.lib_path,
        output_dir=args.output_dir,
        fs=args.fs,
        duration_s=args.duration_s,
        sine_hz=args.sine_hz,
        seed=args.seed,
        reference_gain=args.reference_gain,
        save_wav=not args.no_wav,
    )
    print(json.dumps(metrics, indent=2))


if __name__ == "__main__":
    main()

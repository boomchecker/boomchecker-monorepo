from __future__ import annotations

"""Synthetic filtered-x LMS ANC demo and host-side test harness.

The Python side generates the reference signal x[n] and the primary acoustic
path P(z), producing d[n] = P(z)x[n]. The C core owns the adaptive controller
G(z), the demo secondary path C(z), and the secondary-path estimate C_hat(z)
used by the filtered-x update.
"""

import argparse
import ctypes
import json
import os
from math import gcd
from pathlib import Path

os.environ.setdefault(
    "MPLCONFIGDIR", str(Path(__file__).resolve().parent.parent / "build" / "mplconfig")
)

import matplotlib

matplotlib.use("Agg")

import matplotlib.pyplot as plt
import numpy as np
from scipy.io.wavfile import read, write
from scipy.signal import butter, lfilter, resample_poly, welch


ROOT_DIR = Path(__file__).resolve().parent.parent
DEFAULT_LIB_PATH = ROOT_DIR / "build" / "liblms_filter.so"
DEFAULT_OUTPUT_DIR = Path(__file__).resolve().parent / "out"


def bandpass_noise(
    rng: np.random.Generator,
    num_samples: int,
    fs: int,
    low_hz: float,
    high_hz: float,
    gain: float,
) -> np.ndarray:
    noise = rng.standard_normal(num_samples)
    high_hz = min(high_hz, 0.95 * fs / 2)
    b, a = butter(4, [low_hz / (fs / 2), high_hz / (fs / 2)], btype="bandpass")
    return gain * lfilter(b, a, noise)


def normalize_audio(x: np.ndarray, peak: float = 0.85) -> np.ndarray:
    max_val = float(np.max(np.abs(x)))
    if max_val < 1e-12:
        return x.astype(np.float32)
    return (peak * x / max_val).astype(np.float32)


def wav_to_float32(samples: np.ndarray) -> np.ndarray:
    if np.issubdtype(samples.dtype, np.floating):
        converted = np.nan_to_num(samples.astype(np.float32))
    elif np.issubdtype(samples.dtype, np.unsignedinteger):
        info = np.iinfo(samples.dtype)
        midpoint = (float(info.max) + 1.0) / 2.0
        converted = ((samples.astype(np.float32) - midpoint) / midpoint).astype(np.float32)
    elif np.issubdtype(samples.dtype, np.signedinteger):
        info = np.iinfo(samples.dtype)
        peak = float(max(abs(info.min), info.max))
        converted = (samples.astype(np.float32) / peak).astype(np.float32)
    else:
        raise TypeError(f"Unsupported WAV dtype: {samples.dtype}")

    if converted.ndim > 1:
        converted = converted.mean(axis=1)
    return converted.astype(np.float32)


def condition_wanted_wav(
    wav_path: Path,
    fs: int,
    duration_s: float,
    gain: float,
    peak: float = 0.70,
) -> np.ndarray:
    source_fs, samples = read(wav_path)
    wanted = wav_to_float32(samples)
    if source_fs != fs:
        sample_rate_gcd = gcd(source_fs, fs)
        wanted = resample_poly(
            wanted,
            fs // sample_rate_gcd,
            source_fs // sample_rate_gcd,
        ).astype(np.float32)

    target_samples = int(fs * duration_s)
    if len(wanted) < target_samples:
        wanted = np.pad(wanted, (0, target_samples - len(wanted)), mode="constant")
    else:
        wanted = wanted[:target_samples]

    return (normalize_audio(wanted, peak=peak) * gain).astype(np.float32)


def generate_rpm_profile(t: np.ndarray, rpm_base: float, rpm_variation: float) -> np.ndarray:
    rpm = (
        rpm_base
        + rpm_variation * np.sin(2 * np.pi * 0.18 * t)
        + 0.35 * rpm_variation * np.sin(2 * np.pi * 0.047 * t + 1.3)
    )
    rpm += 1800 * np.exp(-0.5 * ((t - 1.2) / 0.16) ** 2)
    rpm += -1200 * np.exp(-0.5 * ((t - 2.3) / 0.22) ** 2)
    return rpm


def generate_drone_reference(
    fs: int,
    duration_s: float,
    seed: int,
    rpm_base: float = 8800,
    rpm_variation: float = 1200,
    propeller_blades: int = 2,
) -> np.ndarray:
    samples = int(fs * duration_s)
    t = np.arange(samples) / fs
    rng = np.random.default_rng(seed)

    rpm = generate_rpm_profile(t, rpm_base, rpm_variation)
    blade_pass_freq = propeller_blades * rpm / 60.0
    phase = 2 * np.pi * np.cumsum(blade_pass_freq) / fs

    signal = np.zeros_like(t)
    for harmonic, gain in enumerate([1.0, 0.55, 0.34, 0.22, 0.14, 0.10], start=1):
        signal += gain * np.sin(harmonic * phase + 0.18 * np.sin(2 * np.pi * 0.7 * t))

    amp_mod = 1.0 + 0.16 * np.sin(2 * np.pi * 3.1 * t)
    amp_mod += 0.08 * np.sin(2 * np.pi * 6.8 * t + 0.8)
    amp_mod += 0.22 * np.exp(-0.5 * ((t - 1.2) / 0.20) ** 2)
    amp_mod += 0.18 * np.exp(-0.5 * ((t - 2.3) / 0.28) ** 2)
    signal *= amp_mod

    turbulence = bandpass_noise(rng, samples, fs, 700, 6200, gain=0.26)
    vibration = 0.20 * np.sin(2 * np.pi * np.mean(rpm / 60.0) * t)
    vibration += 0.10 * np.sin(2 * np.pi * 2.0 * np.mean(rpm / 60.0) * t)
    return normalize_audio(signal + turbulence + vibration, peak=0.70)


def apply_primary_path(reference_x: np.ndarray, fs: int) -> np.ndarray:
    """Apply P(z), a Python-only primary path with delay and FIR distortion.

    FIR tap order is newest-sample-first: tap i multiplies x[n - i]. The delay
    stands in for acoustic propagation from drone/reference pickup to the error
    microphone.
    """
    delay_samples = max(1, int(round(0.0015 * fs)))
    primary_path = np.array([0.48, -0.16, 0.10, 0.06, -0.035, 0.02], dtype=np.float32)
    delayed = np.pad(reference_x, (delay_samples, 0), mode="constant")[: len(reference_x)]
    return lfilter(primary_path, [1.0], delayed).astype(np.float32)


def to_q15(x: np.ndarray, peak: float = 0.85) -> np.ndarray:
    max_val = float(np.max(np.abs(x)))
    if max_val > peak:
        x = x * (peak / max_val)
    return np.clip(np.round(x * 32767.0), -32768, 32767).astype(np.int16)


def from_q15(x: np.ndarray) -> np.ndarray:
    return x.astype(np.float32) / 32768.0


def load_fxlms_lib(lib_path: Path) -> ctypes.CDLL:
    if not lib_path.exists():
        raise FileNotFoundError(f"FXLMS library not found: {lib_path}. Run `task build` first.")
    lib = ctypes.CDLL(str(lib_path))
    ptr_i16 = np.ctypeslib.ndpointer(dtype=np.int16, ndim=1, flags="C_CONTIGUOUS")
    lib.fxlms_filter_i16.argtypes = [
        ptr_i16,
        ptr_i16,
        ptr_i16,
        ptr_i16,
        ptr_i16,
        ctypes.c_size_t,
    ]
    lib.fxlms_filter_i16.restype = ctypes.c_int
    return lib


def run_fxlms(
    lib_path: Path, reference_q15: np.ndarray, primary_q15: np.ndarray
) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    lib = load_fxlms_lib(lib_path)
    error = np.zeros_like(primary_q15)
    controller = np.zeros_like(primary_q15)
    secondary = np.zeros_like(primary_q15)
    status = lib.fxlms_filter_i16(
        reference_q15, primary_q15, error, controller, secondary, primary_q15.size
    )
    if status != 0:
        raise RuntimeError(f"fxlms_filter_i16 failed with status {status}")
    return error, controller, secondary


def mse(x: np.ndarray) -> float:
    return float(np.mean(x.astype(np.float64) ** 2))


def plot_results(
    out_path: Path,
    fs: int,
    reference_x: np.ndarray,
    primary_d: np.ndarray,
    wanted: np.ndarray,
    controller_y: np.ndarray,
    secondary_output: np.ndarray,
    error_e: np.ndarray,
    seconds: float = 0.15,
) -> None:
    count = min(len(reference_x), int(seconds * fs))
    t = np.arange(count) / fs
    has_wanted = bool(np.max(np.abs(wanted)) > 1e-12)
    row_count = 6 if has_wanted else 5
    fig, axes = plt.subplots(
        row_count,
        1,
        figsize=(12, 10 if has_wanted else 9),
        sharex=True,
    )
    series = [
        ("reference x[n]", reference_x),
        ("primary d[n] = P(z)x[n]", primary_d),
    ]
    if has_wanted:
        series.append(("wanted signal", wanted))
    series.extend(
        [
            ("controller y[n] = G(z)x[n]", controller_y),
            ("secondary C(z)y[n]", secondary_output),
            ("error e[n] = d[n] - C(z)y[n]", error_e),
        ]
    )
    for ax, (title, values) in zip(axes, series):
        ax.plot(t, values[:count])
        ax.set_title(title)
        ax.grid(True, alpha=0.25)
        ax.set_ylim(-1.05, 1.05)
    axes[-1].set_xlabel("Time [s]")
    fig.tight_layout()
    fig.savefig(out_path, dpi=140)
    plt.close(fig)


def spectrum_db(x: np.ndarray, fs: int) -> tuple[np.ndarray, np.ndarray]:
    nperseg = min(4096, len(x))
    if nperseg < 8:
        freqs = np.fft.rfftfreq(len(x), d=1.0 / fs)
        power = np.abs(np.fft.rfft(x)) ** 2
        return freqs, 10.0 * np.log10(np.maximum(power, 1e-20))

    freqs, psd = welch(
        x,
        fs=fs,
        window="hann",
        nperseg=nperseg,
        noverlap=nperseg // 2,
        scaling="density",
    )
    return freqs, 10.0 * np.log10(np.maximum(psd, 1e-20))


def plot_spectrum_results(
    out_path: Path,
    fs: int,
    drone_primary: np.ndarray,
    noise_residual: np.ndarray,
) -> None:
    freqs, drone_db = spectrum_db(drone_primary, fs)
    _, residual_db = spectrum_db(noise_residual, fs)
    attenuation_db = drone_db - residual_db

    fig, axes = plt.subplots(2, 1, figsize=(12, 8), sharex=True)
    axes[0].plot(freqs, drone_db, label="drone before ANC")
    axes[0].plot(freqs, residual_db, label="drone residual after ANC")
    axes[0].set_ylabel("PSD [dB/Hz]")
    axes[0].set_title("Drone noise spectrum")
    axes[0].grid(True, alpha=0.25)
    axes[0].legend(loc="best")

    axes[1].plot(freqs, attenuation_db)
    axes[1].axhline(0.0, color="black", linewidth=0.8, alpha=0.55)
    axes[1].set_ylabel("Attenuation [dB]")
    axes[1].set_xlabel("Frequency [Hz]")
    axes[1].set_title("Frequency-dependent attenuation")
    axes[1].grid(True, alpha=0.25)
    axes[1].set_xlim(0, fs / 2)

    fig.tight_layout()
    fig.savefig(out_path, dpi=140)
    plt.close(fig)


def build_demo(
    fs: int,
    duration_s: float,
    seed: int,
    wanted_wav_path: Path | None = None,
    wanted_gain: float = 0.35,
) -> dict[str, np.ndarray]:
    reference = generate_drone_reference(fs=fs, duration_s=duration_s, seed=seed)
    drone_primary = apply_primary_path(reference, fs)
    if wanted_wav_path is None:
        wanted = np.zeros_like(drone_primary)
    else:
        wanted = condition_wanted_wav(wanted_wav_path, fs, duration_s, wanted_gain)
    primary = drone_primary + wanted

    peak = max(
        float(np.max(np.abs(reference))),
        float(np.max(np.abs(drone_primary))),
        float(np.max(np.abs(wanted))),
        float(np.max(np.abs(primary))),
    )
    if peak > 0.85:
        scale = 0.85 / peak
        reference = reference * scale
        drone_primary = drone_primary * scale
        wanted = wanted * scale
        primary = primary * scale
    return {
        "reference_x": reference.astype(np.float32),
        "drone_primary": drone_primary.astype(np.float32),
        "wanted": wanted.astype(np.float32),
        "primary_d": primary.astype(np.float32),
    }


def run_demo(
    lib_path: Path = DEFAULT_LIB_PATH,
    output_dir: Path = DEFAULT_OUTPUT_DIR,
    fs: int = 16000,
    duration_s: float = 4.0,
    seed: int = 7,
    reference_gain: float = 1.0,
    wanted_wav_path: Path | None = None,
    wanted_gain: float = 0.35,
    save_wav: bool = True,
) -> dict[str, float | str]:
    output_dir.mkdir(parents=True, exist_ok=True)
    data = build_demo(
        fs=fs,
        duration_s=duration_s,
        seed=seed,
        wanted_wav_path=wanted_wav_path,
        wanted_gain=wanted_gain,
    )
    reference_q15 = to_q15(data["reference_x"] * reference_gain, peak=0.9)
    primary_q15 = to_q15(data["primary_d"], peak=0.9)
    drone_primary_q15 = to_q15(data["drone_primary"], peak=0.9)
    wanted_q15 = to_q15(data["wanted"], peak=0.9)
    error_q15, controller_q15, secondary_q15 = run_fxlms(
        lib_path, reference_q15, primary_q15
    )

    reference = from_q15(reference_q15)
    primary = from_q15(primary_q15)
    drone_primary = from_q15(drone_primary_q15)
    wanted = from_q15(wanted_q15)
    error = from_q15(error_q15)
    controller = from_q15(controller_q15)
    secondary = from_q15(secondary_q15)
    noise_residual = error - wanted

    settle = min(len(primary) // 2, int(fs * 1.0))
    tail = slice(settle, None)
    metrics = {
        "primary_mse": mse(primary),
        "error_mse": mse(error),
        "primary_tail_mse": mse(primary[tail]),
        "error_tail_mse": mse(error[tail]),
        "attenuation_tail_db": 10.0
        * np.log10(max(mse(primary[tail]), 1e-20) / max(mse(error[tail]), 1e-20)),
        "drone_primary_tail_mse": mse(drone_primary[tail]),
        "noise_residual_tail_mse": mse(noise_residual[tail]),
        "drone_attenuation_tail_db": 10.0
        * np.log10(
            max(mse(drone_primary[tail]), 1e-20)
            / max(mse(noise_residual[tail]), 1e-20)
        ),
    }

    plot_path = output_dir / "fxlms_demo.png"
    plot_results(plot_path, fs, reference, primary, wanted, controller, secondary, error)
    spectrum_path = output_dir / "fxlms_spectrum.png"
    plot_spectrum_results(
        spectrum_path,
        fs,
        drone_primary[tail],
        noise_residual[tail],
    )

    metrics_path = output_dir / "metrics.json"
    metrics_with_paths: dict[str, float | str] = dict(metrics)
    metrics_with_paths["plot_path"] = str(plot_path)
    metrics_with_paths["spectrum_path"] = str(spectrum_path)
    metrics_with_paths["reference_gain"] = reference_gain
    metrics_with_paths["wanted_gain"] = wanted_gain
    if wanted_wav_path is not None:
        metrics_with_paths["wanted_wav_path"] = str(wanted_wav_path)
    metrics_path.write_text(json.dumps(metrics_with_paths, indent=2), encoding="utf-8")

    if save_wav:
        write(output_dir / "reference.wav", fs, reference_q15)
        write(output_dir / "primary_d.wav", fs, primary_q15)
        write(output_dir / "controller_y.wav", fs, controller_q15)
        write(output_dir / "secondary_output.wav", fs, secondary_q15)
        write(output_dir / "error.wav", fs, error_q15)
        if wanted_wav_path is not None:
            write(output_dir / "wanted.wav", fs, wanted_q15)
            write(output_dir / "drone_primary.wav", fs, drone_primary_q15)
            write(
                output_dir / "noise_residual.wav",
                fs,
                to_q15(noise_residual, peak=0.9),
            )

    return metrics_with_paths


def main() -> None:
    parser = argparse.ArgumentParser(description="Run synthetic Q15 FXLMS ANC demo.")
    parser.add_argument("--lib-path", type=Path, default=DEFAULT_LIB_PATH)
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT_DIR)
    parser.add_argument("--fs", type=int, default=16000)
    parser.add_argument("--duration-s", type=float, default=4.0)
    parser.add_argument("--seed", type=int, default=7)
    parser.add_argument("--reference-gain", type=float, default=1.0)
    parser.add_argument("--wanted-wav", type=Path)
    parser.add_argument("--wanted-gain", type=float, default=0.35)
    parser.add_argument("--no-wav", action="store_true")
    args = parser.parse_args()

    metrics = run_demo(
        lib_path=args.lib_path,
        output_dir=args.output_dir,
        fs=args.fs,
        duration_s=args.duration_s,
        seed=args.seed,
        reference_gain=args.reference_gain,
        wanted_wav_path=args.wanted_wav,
        wanted_gain=args.wanted_gain,
        save_wav=not args.no_wav,
    )
    print(json.dumps(metrics, indent=2))


if __name__ == "__main__":
    main()

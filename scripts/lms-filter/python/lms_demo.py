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

plt.rcParams.update(
    {
        "savefig.dpi": 200,
        "font.size": 11,
        "axes.titlesize": 11,
        "axes.titleweight": "bold",
        "axes.labelsize": 10,
        "axes.grid": True,
        "grid.alpha": 0.25,
        "grid.linewidth": 0.6,
        "axes.spines.top": False,
        "axes.spines.right": False,
        "legend.fontsize": 8,
        "legend.framealpha": 0.85,
        "legend.handlelength": 1.4,
        "legend.columnspacing": 1.0,
        "lines.linewidth": 0.9,
    }
)


ROOT_DIR = Path(__file__).resolve().parent.parent
DEFAULT_LIB_PATH = ROOT_DIR / "build" / "liblms_filter.so"
DEFAULT_OUTPUT_DIR = Path(__file__).resolve().parent / "out"
DEFAULT_PLOT_WINDOW = (0.0, 0.15)


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


def sum_q15_channels(x: np.ndarray) -> np.ndarray:
    summed = np.sum(x.astype(np.int32), axis=0)
    return np.clip(summed, -32768, 32767).astype(np.int16)


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
    lib.fxlms_filter_multi_i16.argtypes = [
        ptr_i16,
        ptr_i16,
        ctypes.c_uint8,
        ctypes.c_uint8,
        ptr_i16,
        ptr_i16,
        ptr_i16,
        ctypes.c_size_t,
    ]
    lib.fxlms_filter_multi_i16.restype = ctypes.c_int
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


def run_fxlms_multi(
    lib_path: Path,
    references_q15: np.ndarray,
    primary_q15: np.ndarray,
    actuator_count: int,
) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    lib = load_fxlms_lib(lib_path)
    references_q15 = np.ascontiguousarray(references_q15, dtype=np.int16)
    primary_q15 = np.ascontiguousarray(primary_q15, dtype=np.int16)
    if references_q15.ndim != 2:
        raise ValueError("references_q15 must have shape (reference_count, samples)")
    reference_count, sample_count = references_q15.shape
    if primary_q15.shape != (sample_count,):
        raise ValueError("primary_q15 length must match references_q15 samples")
    error = np.zeros_like(primary_q15)
    controller = np.zeros((actuator_count, sample_count), dtype=np.int16)
    secondary = np.zeros((actuator_count, sample_count), dtype=np.int16)
    status = lib.fxlms_filter_multi_i16(
        references_q15.reshape(-1),
        primary_q15,
        reference_count,
        actuator_count,
        error,
        controller.reshape(-1),
        secondary.reshape(-1),
        primary_q15.size,
    )
    if status != 0:
        raise RuntimeError(f"fxlms_filter_multi_i16 failed with status {status}")
    return error, controller, secondary


def mse(x: np.ndarray) -> float:
    return float(np.mean(x.astype(np.float64) ** 2))


def parse_plot_window(value: str) -> tuple[float, float]:
    if ".." in value:
        parts = value.split("..")
        if len(parts) != 2:
            raise argparse.ArgumentTypeError(
                "Plot window must be a duration like '2' or an interval like '1.5..2'."
            )
        try:
            start_s = float(parts[0])
            end_s = float(parts[1])
        except ValueError as exc:
            raise argparse.ArgumentTypeError(
                "Plot interval bounds must be numbers."
            ) from exc
    else:
        try:
            start_s = 0.0
            end_s = float(value)
        except ValueError as exc:
            raise argparse.ArgumentTypeError("Plot duration must be a number.") from exc

    if start_s < 0.0 or end_s <= start_s:
        raise argparse.ArgumentTypeError(
            "Plot window must satisfy 0 <= start < end."
        )
    return start_s, end_s


def analysis_slice(
    sample_count: int,
    fs: int,
    spectrum_window: tuple[float, float] | None = None,
) -> tuple[slice, float, float]:
    if spectrum_window is None:
        settle = min(sample_count // 2, int(fs * 1.0))
        return slice(settle, None), settle / fs, sample_count / fs

    start_s, end_s = spectrum_window
    start_idx = min(sample_count, int(start_s * fs))
    end_idx = min(sample_count, int(end_s * fs))
    if end_idx <= start_idx:
        raise ValueError("--spectrum-window must overlap the generated signal")
    return slice(start_idx, end_idx), start_idx / fs, end_idx / fs


def plot_results(
    out_path: Path,
    fs: int,
    reference_x: np.ndarray,
    primary_d: np.ndarray,
    wanted: np.ndarray,
    controller_y: np.ndarray,
    secondary_output: np.ndarray,
    error_e: np.ndarray,
    plot_window: tuple[float, float] = DEFAULT_PLOT_WINDOW,
) -> None:
    start_s, end_s = plot_window
    start_idx = min(len(reference_x), int(start_s * fs))
    end_idx = min(len(reference_x), int(end_s * fs))
    if end_idx <= start_idx:
        start_idx = 0
        end_idx = min(len(reference_x), max(1, int(DEFAULT_PLOT_WINDOW[1] * fs)))
    t = np.arange(start_idx, end_idx) / fs
    has_wanted = bool(np.max(np.abs(wanted)) > 1e-12)
    row_count = 7 if has_wanted else 5
    fig, axes = plt.subplots(
        row_count,
        1,
        figsize=(12, 12 if has_wanted else 9),
        sharex=True,
    )
    series = [
        ("reference x[n]", reference_x),
        ("primary d[n] = P(z)x[n]", primary_d),
    ]
    if has_wanted:
        series.append(("wanted signal", wanted))
        series.append(("wanted - error", wanted - error_e))
    series.extend(
        [
            ("controller y[n] = G(z)x[n]", controller_y),
            ("secondary C(z)y[n]", secondary_output),
            ("error e[n] = d[n] - C(z)y[n]", error_e),
        ]
    )
    for ax, (title, values) in zip(axes, series):
        ax.plot(t, values[start_idx:end_idx])
        ax.set_title(title)
        ax.grid(True, alpha=0.25)
        ax.set_ylim(-1.05, 1.05)
    axes[-1].set_xlabel("Time [s]")
    fig.tight_layout()
    fig.savefig(out_path, dpi=140)
    plt.close(fig)


def _autoscale_symmetric(ax: plt.Axes, *arrays: np.ndarray, margin: float = 1.2,
                         floor: float = 1e-3) -> None:
    """Set a tight symmetric y-limit so small signals stay visible."""
    peak = 0.0
    for arr in arrays:
        arr = np.asarray(arr)
        if arr.size:
            peak = max(peak, float(np.max(np.abs(arr))))
    peak = max(peak * margin, floor)
    ax.set_ylim(-peak, peak)


def _window_indices(
    sample_count: int, fs: int, plot_window: tuple[float, float]
) -> tuple[int, int]:
    start_s, end_s = plot_window
    start_idx = min(sample_count, int(start_s * fs))
    end_idx = min(sample_count, int(end_s * fs))
    if end_idx <= start_idx:
        start_idx = 0
        end_idx = min(sample_count, max(1, int(DEFAULT_PLOT_WINDOW[1] * fs)))
    return start_idx, end_idx


def plot_multichannel_results(
    out_path: Path,
    fs: int,
    references: np.ndarray,
    primary_d: np.ndarray,
    wanted: np.ndarray,
    controller_y: np.ndarray,
    secondary_sum: np.ndarray,
    error_e: np.ndarray,
    plot_window: tuple[float, float] = DEFAULT_PLOT_WINDOW,
) -> None:
    start_idx, end_idx = _window_indices(len(primary_d), fs, plot_window)
    sl = slice(start_idx, end_idx)
    t = np.arange(start_idx, end_idx) / fs
    has_wanted = bool(np.max(np.abs(wanted)) > 1e-12)

    fig, axes = plt.subplots(4, 1, figsize=(7.1, 6.2), sharex=True)
    ref_count = max(references.shape[0], 1)
    act_count = max(controller_y.shape[0], 1)
    ref_colors = plt.cm.viridis(np.linspace(0.12, 0.82, ref_count))
    act_colors = plt.cm.plasma(np.linspace(0.12, 0.78, act_count))

    # (1) Reference channels from the motors.
    ax = axes[0]
    for i, values in enumerate(references):
        ax.plot(t, values[sl], color=ref_colors[i], lw=0.7, alpha=0.9,
                label=fr"$x_{{{i + 1}}}$")
    ax.set_ylabel("references")
    _autoscale_symmetric(ax, references[:, sl])
    ax.legend(loc="upper right", ncol=ref_count)

    # (2) Microphone signal vs. the summed anti-noise that cancels it.
    ax = axes[1]
    ax.plot(t, primary_d[sl], color="#1f77b4", lw=0.8, label=r"mic $d[n]$")
    ax.plot(t, secondary_sum[sl], color="#d62728", lw=0.8, alpha=0.85,
            label=r"anti-noise $\sum_a C_a y_a$")
    ax.set_ylabel("mic / anti-noise")
    _autoscale_symmetric(ax, primary_d[sl], secondary_sum[sl])
    ax.legend(loc="upper right", ncol=2)

    # (3) Per-actuator controller outputs (each y_a drives its own actuator).
    ax = axes[2]
    for a, values in enumerate(controller_y):
        ax.plot(t, values[sl], color=act_colors[a], lw=0.7, alpha=0.9,
                label=fr"$y_{{{a + 1}}}$")
    ax.set_ylabel("actuators")
    _autoscale_symmetric(ax, controller_y[:, sl])
    ax.legend(loc="upper right", ncol=act_count)

    # (4) ANC output (error) with the useful signal it must preserve.
    ax = axes[3]
    if has_wanted:
        ax.plot(t, wanted[sl], color="0.6", lw=0.9, label=r"wanted $s[n]$")
    ax.plot(t, error_e[sl], color="#2ca02c", lw=0.8, label=r"ANC output $e[n]$")
    ax.set_ylabel("output")
    _autoscale_symmetric(ax, error_e[sl], wanted[sl] if has_wanted else error_e[sl])
    ax.legend(loc="upper right", ncol=2 if has_wanted else 1)

    axes[0].set_xlim(t[0], t[-1])
    axes[-1].set_xlabel("Time [s]")
    fig.align_ylabels(axes)
    fig.tight_layout(h_pad=0.6)
    fig.savefig(out_path)
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


def octave_smooth(freqs: np.ndarray, values_db: np.ndarray,
                  fraction: float = 1.0 / 12.0) -> np.ndarray:
    """Fractional-octave moving average for readable acoustic spectra."""
    out = np.asarray(values_db, dtype=np.float64).copy()
    positive = freqs > 0
    f = freqs[positive]
    if f.size < 2:
        return out
    values = out[positive]
    cumulative = np.concatenate([[0.0], np.cumsum(values)])
    factor = 2.0 ** (fraction / 2.0)
    lo = np.searchsorted(f, f / factor, side="left")
    hi = np.searchsorted(f, f * factor, side="right")
    counts = np.maximum(hi - lo, 1)
    out[positive] = (cumulative[hi] - cumulative[lo]) / counts
    return out


def plot_spectrum_results(
    out_path: Path,
    fs: int,
    wanted: np.ndarray,
    primary_d: np.ndarray,
    error_e: np.ndarray,
    drone_primary: np.ndarray,
    noise_residual: np.ndarray,
) -> None:
    freqs, primary_db = spectrum_db(primary_d, fs)
    _, error_db = spectrum_db(error_e, fs)
    _, wanted_db = spectrum_db(wanted, fs)
    _, drone_db = spectrum_db(drone_primary, fs)
    _, residual_db = spectrum_db(noise_residual, fs)

    primary_s = octave_smooth(freqs, primary_db)
    error_s = octave_smooth(freqs, error_db)
    wanted_s = octave_smooth(freqs, wanted_db)
    drone_s = octave_smooth(freqs, drone_db)
    residual_s = octave_smooth(freqs, residual_db)
    attenuation_s = drone_s - residual_s

    has_wanted = bool(np.max(np.abs(wanted)) > 1e-12)
    f_lo = max(20.0, float(freqs[1]) if freqs.size > 1 else 20.0)
    f_hi = fs / 2.0
    positive = freqs > 0

    fig, axes = plt.subplots(2, 1, figsize=(7.1, 4.8), sharex=True)

    ax = axes[0]
    ax.semilogx(freqs, primary_s, color="#d62728", label=r"noisy input $d[n]$")
    ax.semilogx(freqs, error_s, color="#2ca02c", label=r"ANC output $e[n]$")
    if has_wanted:
        ax.semilogx(freqs, wanted_s, color="0.55", lw=0.9, label=r"wanted $s[n]$")
    ax.set_ylabel("PSD [dB/Hz]")
    ax.set_title("Spectrum: microphone vs. ANC output")
    ax.legend(loc="lower left", ncol=3)

    # Full attenuation curve, including frequencies where the controller ADDS
    # energy (negative). Green = reduced, red = added. The added levels sit far
    # below the drone tones (see top panel), so they barely affect the average.
    ax = axes[1]
    ax.set_xscale("log")
    f_pos = freqs[positive]
    att_pos = attenuation_s[positive]
    ax.fill_between(f_pos, att_pos, 0.0, where=att_pos >= 0.0,
                    color="#2ca02c", alpha=0.25, interpolate=True)
    ax.fill_between(f_pos, att_pos, 0.0, where=att_pos < 0.0,
                    color="#d62728", alpha=0.25, interpolate=True)
    ax.plot(f_pos, att_pos, color="#1f77b4", lw=1.0)
    ax.axhline(0.0, color="black", lw=0.8, alpha=0.6)
    ax.set_ylabel("Attenuation [dB]")
    ax.set_xlabel("Frequency [Hz]")
    ax.set_title("Drone attenuation (green = reduced, red = added)")

    axes[0].set_xlim(f_lo, f_hi)
    fig.align_ylabels(axes)
    fig.tight_layout(h_pad=0.6)
    fig.savefig(out_path)
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


def apply_motor_primary_path(reference_x: np.ndarray, fs: int, motor: int) -> np.ndarray:
    delay_samples = max(1, int(round((0.0012 + 0.00018 * motor) * fs)))
    paths = [
        np.array([0.44, -0.13, 0.08, 0.045, -0.025, 0.015], dtype=np.float32),
        np.array([0.39, -0.11, 0.09, 0.052, -0.030, 0.012], dtype=np.float32),
        np.array([0.42, -0.15, 0.075, 0.040, -0.020, 0.017], dtype=np.float32),
        np.array([0.36, -0.10, 0.070, 0.050, -0.024, 0.010], dtype=np.float32),
    ]
    delayed = np.pad(reference_x, (delay_samples, 0), mode="constant")[: len(reference_x)]
    return lfilter(paths[motor], [1.0], delayed).astype(np.float32)


def build_multimotor_demo(
    fs: int,
    duration_s: float,
    seed: int,
    motor_count: int = 4,
) -> dict[str, np.ndarray]:
    references = []
    primaries = []
    rpm_offsets = [-260.0, -80.0, 130.0, 310.0]
    variation_scales = [0.92, 1.05, 0.98, 1.10]
    gains = [1.00, 0.92, 0.88, 0.84]
    for motor in range(motor_count):
        reference = generate_drone_reference(
            fs=fs,
            duration_s=duration_s,
            seed=seed + 101 * motor,
            rpm_base=8800.0 + rpm_offsets[motor],
            rpm_variation=1200.0 * variation_scales[motor],
        )
        reference = reference * gains[motor]
        references.append(reference.astype(np.float32))
        primaries.append(apply_motor_primary_path(reference, fs, motor))

    reference_matrix = np.stack(references).astype(np.float32)
    drone_primary = np.sum(np.stack(primaries), axis=0).astype(np.float32)
    summed_reference = np.sum(reference_matrix, axis=0).astype(np.float32)
    peak = max(
        float(np.max(np.abs(reference_matrix))),
        float(np.max(np.abs(summed_reference))),
        float(np.max(np.abs(drone_primary))),
    )
    if peak > 0.85:
        scale = 0.85 / peak
        reference_matrix *= scale
        summed_reference *= scale
        drone_primary *= scale
    return {
        "references": reference_matrix,
        "summed_reference": summed_reference,
        "drone_primary": drone_primary,
        "primary_d": drone_primary.copy(),
    }


def run_multichannel_comparison(
    lib_path: Path = DEFAULT_LIB_PATH,
    output_dir: Path = DEFAULT_OUTPUT_DIR / "multi",
    fs: int = 16000,
    duration_s: float = 4.0,
    seed: int = 7,
    save_wav: bool = True,
) -> dict[str, float | str]:
    output_dir.mkdir(parents=True, exist_ok=True)
    data = build_multimotor_demo(fs=fs, duration_s=duration_s, seed=seed)
    primary_q15 = to_q15(data["primary_d"], peak=0.9)
    drone_primary = from_q15(primary_q15)

    sum_reference_q15 = to_q15(data["summed_reference"][np.newaxis, :], peak=0.9)
    multi_references_q15 = to_q15(data["references"], peak=0.9)

    sum_error_q15, _sum_controller_q15, sum_secondary_q15 = run_fxlms_multi(
        lib_path, sum_reference_q15, primary_q15, actuator_count=4
    )
    multi_error_q15, _multi_controller_q15, multi_secondary_q15 = run_fxlms_multi(
        lib_path, multi_references_q15, primary_q15, actuator_count=4
    )

    sum_error = from_q15(sum_error_q15)
    multi_error = from_q15(multi_error_q15)
    settle = min(len(drone_primary) // 2, int(fs * 1.0))
    tail = slice(settle, None)
    metrics: dict[str, float | str] = {
        "multi_mode": "synthetic_4_motor_4_actuator",
        "primary_tail_mse": mse(drone_primary[tail]),
        "sum_first_tail_mse": mse(sum_error[tail]),
        "multi_ref_tail_mse": mse(multi_error[tail]),
        "sum_first_attenuation_tail_db": 10.0
        * np.log10(max(mse(drone_primary[tail]), 1e-20) / max(mse(sum_error[tail]), 1e-20)),
        "multi_ref_attenuation_tail_db": 10.0
        * np.log10(
            max(mse(drone_primary[tail]), 1e-20) / max(mse(multi_error[tail]), 1e-20)
        ),
        "reference_count": float(multi_references_q15.shape[0]),
        "actuator_count": 4.0,
    }

    metrics_path = output_dir / "metrics.json"
    metrics_with_paths: dict[str, float | str] = dict(metrics)
    metrics_with_paths["metrics_path"] = str(metrics_path)
    metrics_path.write_text(json.dumps(metrics_with_paths, indent=2), encoding="utf-8")

    if save_wav:
        write(output_dir / "d.wav", fs, primary_q15)
        write(output_dir / "sum_first_error.wav", fs, sum_error_q15)
        write(output_dir / "multi_ref_error.wav", fs, multi_error_q15)
        write(output_dir / "sum_first_secondary_sum.wav", fs, sum_q15_channels(sum_secondary_q15))
        write(output_dir / "multi_ref_secondary_sum.wav", fs, sum_q15_channels(multi_secondary_q15))

    return metrics_with_paths


def build_multichannel_mode_demo(
    fs: int,
    duration_s: float,
    seed: int,
    reference_count: int,
    wanted_wav_path: Path | None = None,
    wanted_gain: float = 0.35,
) -> dict[str, np.ndarray]:
    data = build_multimotor_demo(
        fs=fs,
        duration_s=duration_s,
        seed=seed,
        motor_count=reference_count,
    )
    if wanted_wav_path is None:
        wanted = np.zeros_like(data["drone_primary"])
    else:
        wanted = condition_wanted_wav(wanted_wav_path, fs, duration_s, wanted_gain)
    primary = data["drone_primary"] + wanted
    peak = max(
        float(np.max(np.abs(data["references"]))),
        float(np.max(np.abs(data["summed_reference"]))),
        float(np.max(np.abs(data["drone_primary"]))),
        float(np.max(np.abs(wanted))),
        float(np.max(np.abs(primary))),
    )
    if peak > 0.85:
        scale = 0.85 / peak
        data["references"] *= scale
        data["summed_reference"] *= scale
        data["drone_primary"] *= scale
        wanted *= scale
        primary *= scale
    data["wanted"] = wanted.astype(np.float32)
    data["primary_d"] = primary.astype(np.float32)
    return data


def run_multichannel_mode(
    mode: str,
    lib_path: Path = DEFAULT_LIB_PATH,
    output_dir: Path = DEFAULT_OUTPUT_DIR,
    fs: int = 16000,
    duration_s: float = 4.0,
    seed: int = 7,
    reference_count: int = 4,
    actuator_count: int = 4,
    wanted_wav_path: Path | None = None,
    wanted_gain: float = 0.35,
    plot_window: tuple[float, float] = DEFAULT_PLOT_WINDOW,
    spectrum_window: tuple[float, float] | None = None,
    plot_duration_s: float | None = None,
    save_wav: bool = True,
) -> dict[str, float | str]:
    if mode not in {"sum-first", "miso"}:
        raise ValueError(f"Unsupported multi-channel mode: {mode}")
    if reference_count < 1 or reference_count > 4:
        raise ValueError("--reference-count must be in range 1..4")
    if actuator_count < 1 or actuator_count > 4:
        raise ValueError("--actuator-count must be in range 1..4")

    output_dir.mkdir(parents=True, exist_ok=True)
    data = build_multichannel_mode_demo(
        fs=fs,
        duration_s=duration_s,
        seed=seed,
        reference_count=reference_count,
        wanted_wav_path=wanted_wav_path,
        wanted_gain=wanted_gain,
    )
    primary_q15 = to_q15(data["primary_d"], peak=0.9)
    drone_primary_q15 = to_q15(data["drone_primary"], peak=0.9)
    wanted_q15 = to_q15(data["wanted"], peak=0.9)
    if mode == "sum-first":
        references_q15 = to_q15(data["summed_reference"][np.newaxis, :], peak=0.9)
    else:
        references_q15 = to_q15(data["references"], peak=0.9)

    error_q15, controller_q15, secondary_q15 = run_fxlms_multi(
        lib_path, references_q15, primary_q15, actuator_count=actuator_count
    )

    primary = from_q15(primary_q15)
    drone_primary = from_q15(drone_primary_q15)
    wanted = from_q15(wanted_q15)
    error = from_q15(error_q15)
    references = from_q15(references_q15)
    controller = from_q15(controller_q15)
    secondary_sum_q15 = sum_q15_channels(secondary_q15)
    secondary_sum = from_q15(secondary_sum_q15)
    controller_sum_q15 = sum_q15_channels(controller_q15)
    noise_residual = error - wanted

    tail, spectrum_start_s, spectrum_end_s = analysis_slice(
        len(primary), fs, spectrum_window
    )
    metrics = {
        "mode": mode,
        "reference_count": float(references_q15.shape[0]),
        "actuator_count": float(actuator_count),
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

    if plot_duration_s is not None:
        plot_window = (0.0, plot_duration_s)
    plot_path = output_dir / "fxlms_demo.png"
    plot_multichannel_results(
        plot_path,
        fs,
        references,
        primary,
        wanted,
        controller,
        secondary_sum,
        error,
        plot_window=plot_window,
    )
    spectrum_path = output_dir / "fxlms_spectrum.png"
    plot_spectrum_results(
        spectrum_path,
        fs,
        wanted[tail],
        primary[tail],
        error[tail],
        drone_primary[tail],
        noise_residual[tail],
    )

    metrics_path = output_dir / "metrics.json"
    metrics_with_paths: dict[str, float | str] = dict(metrics)
    metrics_with_paths["plot_path"] = str(plot_path)
    metrics_with_paths["spectrum_path"] = str(spectrum_path)
    metrics_with_paths["wanted_gain"] = wanted_gain
    metrics_with_paths["plot_start_s"] = plot_window[0]
    metrics_with_paths["plot_end_s"] = plot_window[1]
    metrics_with_paths["plot_duration_s"] = plot_window[1] - plot_window[0]
    metrics_with_paths["spectrum_start_s"] = spectrum_start_s
    metrics_with_paths["spectrum_end_s"] = spectrum_end_s
    metrics_with_paths["spectrum_duration_s"] = spectrum_end_s - spectrum_start_s
    if wanted_wav_path is not None:
        metrics_with_paths["wanted_wav_path"] = str(wanted_wav_path)
    metrics_path.write_text(json.dumps(metrics_with_paths, indent=2), encoding="utf-8")

    if save_wav:
        write(output_dir / "d.wav", fs, primary_q15)
        write(output_dir / "x.wav", fs, references_q15[0])
        write(output_dir / "y.wav", fs, controller_sum_q15)
        write(output_dir / "Cy.wav", fs, secondary_sum_q15)
        write(output_dir / "e.wav", fs, error_q15)
        write(output_dir / "Px.wav", fs, drone_primary_q15)
        if wanted_wav_path is not None:
            write(output_dir / "wanted.wav", fs, wanted_q15)
            write(output_dir / "noise_residual.wav", fs, to_q15(noise_residual, peak=0.9))

    return metrics_with_paths


def run_demo(
    lib_path: Path = DEFAULT_LIB_PATH,
    output_dir: Path = DEFAULT_OUTPUT_DIR,
    fs: int = 16000,
    duration_s: float = 4.0,
    seed: int = 7,
    reference_gain: float = 1.0,
    wanted_wav_path: Path | None = None,
    wanted_gain: float = 0.35,
    plot_window: tuple[float, float] = DEFAULT_PLOT_WINDOW,
    spectrum_window: tuple[float, float] | None = None,
    plot_duration_s: float | None = None,
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

    tail, spectrum_start_s, spectrum_end_s = analysis_slice(
        len(primary), fs, spectrum_window
    )
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
    if plot_duration_s is not None:
        plot_window = (0.0, plot_duration_s)
    plot_results(
        plot_path,
        fs,
        reference,
        primary,
        wanted,
        controller,
        secondary,
        error,
        plot_window=plot_window,
    )
    spectrum_path = output_dir / "fxlms_spectrum.png"
    plot_spectrum_results(
        spectrum_path,
        fs,
        wanted[tail],
        primary[tail],
        error[tail],
        drone_primary[tail],
        noise_residual[tail],
    )

    metrics_path = output_dir / "metrics.json"
    metrics_with_paths: dict[str, float | str] = dict(metrics)
    metrics_with_paths["plot_path"] = str(plot_path)
    metrics_with_paths["spectrum_path"] = str(spectrum_path)
    metrics_with_paths["reference_gain"] = reference_gain
    metrics_with_paths["wanted_gain"] = wanted_gain
    metrics_with_paths["plot_start_s"] = plot_window[0]
    metrics_with_paths["plot_end_s"] = plot_window[1]
    metrics_with_paths["plot_duration_s"] = plot_window[1] - plot_window[0]
    metrics_with_paths["spectrum_start_s"] = spectrum_start_s
    metrics_with_paths["spectrum_end_s"] = spectrum_end_s
    metrics_with_paths["spectrum_duration_s"] = spectrum_end_s - spectrum_start_s
    if wanted_wav_path is not None:
        metrics_with_paths["wanted_wav_path"] = str(wanted_wav_path)
    metrics_path.write_text(json.dumps(metrics_with_paths, indent=2), encoding="utf-8")

    if save_wav:
        write(output_dir / "x.wav", fs, reference_q15)
        write(output_dir / "d.wav", fs, primary_q15)
        write(output_dir / "y.wav", fs, controller_q15)
        write(output_dir / "Cy.wav", fs, secondary_q15)
        write(output_dir / "e.wav", fs, error_q15)
        if wanted_wav_path is not None:
            write(output_dir / "wanted.wav", fs, wanted_q15)
            write(output_dir / "Px.wav", fs, drone_primary_q15)
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
    parser.add_argument(
        "--mode",
        choices=("siso", "sum-first", "miso"),
        default="siso",
        help="Processing mode. SISO uses the original one-reference demo.",
    )
    parser.add_argument("--reference-count", type=int, default=4)
    parser.add_argument("--actuator-count", type=int, default=4)
    parser.add_argument(
        "--multi-channel-comparison",
        action="store_true",
        help="Run the synthetic 4-motor/4-actuator sum-first vs multi-ref comparison.",
    )
    parser.add_argument(
        "--plot-window",
        type=parse_plot_window,
        default=DEFAULT_PLOT_WINDOW,
        metavar="SECONDS|START..END",
        help="Time-domain plot window. Examples: '2' or '1.5..2'.",
    )
    parser.add_argument(
        "--spectrum-window",
        type=parse_plot_window,
        metavar="SECONDS|START..END",
        help="Spectrum/metrics analysis window. Defaults to the post-settling tail.",
    )
    parser.add_argument(
        "--plot-duration-s",
        type=float,
        help="Deprecated alias for --plot-window SECONDS.",
    )
    parser.add_argument("--no-wav", action="store_true")
    args = parser.parse_args()

    if args.multi_channel_comparison:
        metrics = run_multichannel_comparison(
            lib_path=args.lib_path,
            output_dir=args.output_dir,
            fs=args.fs,
            duration_s=args.duration_s,
            seed=args.seed,
            save_wav=not args.no_wav,
        )
    elif args.mode in {"sum-first", "miso"}:
        metrics = run_multichannel_mode(
            mode=args.mode,
            lib_path=args.lib_path,
            output_dir=args.output_dir,
            fs=args.fs,
            duration_s=args.duration_s,
            seed=args.seed,
            reference_count=args.reference_count,
            actuator_count=args.actuator_count,
            wanted_wav_path=args.wanted_wav,
            wanted_gain=args.wanted_gain,
            plot_window=args.plot_window,
            spectrum_window=args.spectrum_window,
            plot_duration_s=args.plot_duration_s,
            save_wav=not args.no_wav,
        )
    else:
        metrics = run_demo(
            lib_path=args.lib_path,
            output_dir=args.output_dir,
            fs=args.fs,
            duration_s=args.duration_s,
            seed=args.seed,
            reference_gain=args.reference_gain,
            wanted_wav_path=args.wanted_wav,
            wanted_gain=args.wanted_gain,
            plot_window=args.plot_window,
            spectrum_window=args.spectrum_window,
            plot_duration_s=args.plot_duration_s,
            save_wav=not args.no_wav,
        )
    print(json.dumps(metrics, indent=2))


if __name__ == "__main__":
    main()

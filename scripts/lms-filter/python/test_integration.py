from __future__ import annotations

from pathlib import Path

import numpy as np
from scipy.io.wavfile import write

from lms_demo import (
    DEFAULT_LIB_PATH,
    parse_plot_window,
    run_demo,
    run_multichannel_mode,
    run_multichannel_comparison,
)


def assert_improves(metrics: dict[str, float | str], label: str) -> None:
    if metrics["error_tail_mse"] >= metrics["primary_tail_mse"]:
        raise SystemExit(
            f"Expected {label} FXLMS residual tail MSE to improve, got "
            f"primary={metrics['primary_tail_mse']} error={metrics['error_tail_mse']}"
        )
    if metrics["attenuation_tail_db"] <= 0.5:
        raise SystemExit(
            f"Expected {label} at least 0.5 dB tail attenuation, "
            f"got {metrics['attenuation_tail_db']}"
        )


def make_wanted_fixture(path: Path) -> None:
    fs = 22050
    t = np.arange(int(fs * 3.4)) / fs
    phase = 2 * np.pi * (210.0 * t + 0.5 * 95.0 * t * t)
    envelope = np.clip(t / 0.18, 0.0, 1.0) * np.clip((3.4 - t) / 0.22, 0.0, 1.0)
    rng = np.random.default_rng(123)
    left = envelope * (
        0.42 * np.sin(phase)
        + 0.22 * np.sin(2 * np.pi * 630.0 * t + 0.4)
        + 0.08 * rng.standard_normal(len(t))
    )
    right = envelope * (
        0.34 * np.sin(phase + 0.5 * np.sin(2 * np.pi * 2.7 * t))
        + 0.20 * np.sin(2 * np.pi * 910.0 * t)
        + 0.06 * rng.standard_normal(len(t))
    )
    stereo = np.column_stack([left, right])
    pcm = np.clip(np.round(stereo * 32767.0), -32768, 32767).astype(np.int16)
    write(path, fs, pcm)


def main() -> None:
    test_root = Path(__file__).resolve().parent / "out" / "test"
    assert_improves(
        run_demo(
            lib_path=DEFAULT_LIB_PATH,
            output_dir=test_root / "drone_only",
            fs=16000,
            duration_s=3.0,
            seed=11,
            reference_gain=1.0,
            save_wav=False,
        ),
        "drone-only",
    )

    fixture_path = test_root / "wanted_fixture.wav"
    fixture_path.parent.mkdir(parents=True, exist_ok=True)
    make_wanted_fixture(fixture_path)

    wav_metrics = run_demo(
        lib_path=DEFAULT_LIB_PATH,
        output_dir=test_root / "with_wanted_wav",
        fs=16000,
        duration_s=3.0,
        seed=11,
        reference_gain=1.0,
        wanted_wav_path=fixture_path,
        wanted_gain=0.35,
        plot_window=parse_plot_window("1.5..2"),
        save_wav=True,
    )
    if wav_metrics["noise_residual_tail_mse"] >= wav_metrics["drone_primary_tail_mse"]:
        raise SystemExit(
            f"Expected WAV-mode drone residual tail MSE to improve, got "
            f"drone={wav_metrics['drone_primary_tail_mse']} "
            f"residual={wav_metrics['noise_residual_tail_mse']}"
        )
    if wav_metrics["drone_attenuation_tail_db"] <= 0.5:
        raise SystemExit(
            f"Expected WAV-mode at least 0.5 dB drone attenuation, "
            f"got {wav_metrics['drone_attenuation_tail_db']}"
        )
    if wav_metrics["plot_start_s"] != 1.5 or wav_metrics["plot_end_s"] != 2.0:
        raise SystemExit(
            f"Expected plot window 1.5..2.0, got "
            f"{wav_metrics['plot_start_s']}..{wav_metrics['plot_end_s']}"
        )
    for filename in (
        "wanted.wav",
        "drone_primary.wav",
        "noise_residual.wav",
        "fxlms_spectrum.png",
    ):
        if not (test_root / "with_wanted_wav" / filename).exists():
            raise SystemExit(f"Expected WAV-mode output {filename} to be written")

    multi_metrics = run_multichannel_comparison(
        lib_path=DEFAULT_LIB_PATH,
        output_dir=test_root / "multi_channel",
        fs=16000,
        duration_s=3.0,
        seed=11,
        save_wav=True,
    )
    if multi_metrics["sum_first_tail_mse"] >= multi_metrics["primary_tail_mse"]:
        raise SystemExit(
            f"Expected sum-first multi-actuator mode to improve, got "
            f"primary={multi_metrics['primary_tail_mse']} "
            f"sum_first={multi_metrics['sum_first_tail_mse']}"
        )
    if multi_metrics["multi_ref_tail_mse"] >= multi_metrics["primary_tail_mse"]:
        raise SystemExit(
            f"Expected multi-reference mode to improve, got "
            f"primary={multi_metrics['primary_tail_mse']} "
            f"multi_ref={multi_metrics['multi_ref_tail_mse']}"
        )
    for filename in (
        "sum_first_error.wav",
        "multi_ref_error.wav",
        "metrics.json",
    ):
        if not (test_root / "multi_channel" / filename).exists():
            raise SystemExit(f"Expected multi-channel output {filename} to be written")

    miso_metrics = run_multichannel_mode(
        mode="miso",
        lib_path=DEFAULT_LIB_PATH,
        output_dir=test_root / "miso_mode",
        fs=16000,
        duration_s=3.0,
        seed=11,
        reference_count=4,
        actuator_count=4,
        wanted_wav_path=fixture_path,
        wanted_gain=0.1,
        plot_window=parse_plot_window("0..3"),
        spectrum_window=parse_plot_window("1..2"),
        save_wav=True,
    )
    if miso_metrics["noise_residual_tail_mse"] >= miso_metrics["drone_primary_tail_mse"]:
        raise SystemExit(
            f"Expected MISO mode drone residual to improve, got "
            f"drone={miso_metrics['drone_primary_tail_mse']} "
            f"residual={miso_metrics['noise_residual_tail_mse']}"
        )
    if miso_metrics["plot_start_s"] != 0.0 or miso_metrics["plot_end_s"] != 3.0:
        raise SystemExit(
            f"Expected MISO plot window 0..3, got "
            f"{miso_metrics['plot_start_s']}..{miso_metrics['plot_end_s']}"
        )
    if miso_metrics["spectrum_start_s"] != 1.0 or miso_metrics["spectrum_end_s"] != 2.0:
        raise SystemExit(
            f"Expected MISO spectrum window 1..2, got "
            f"{miso_metrics['spectrum_start_s']}..{miso_metrics['spectrum_end_s']}"
        )
    print("python integration: ok")


if __name__ == "__main__":
    main()

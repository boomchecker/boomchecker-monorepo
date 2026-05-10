from __future__ import annotations

from pathlib import Path

import numpy as np
from scipy.io.wavfile import write

from lms_demo import DEFAULT_LIB_PATH, run_demo


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
    for filename in ("wanted.wav", "drone_primary.wav", "noise_residual.wav"):
        if not (test_root / "with_wanted_wav" / filename).exists():
            raise SystemExit(f"Expected WAV-mode output {filename} to be written")
    print("python integration: ok")


if __name__ == "__main__":
    main()

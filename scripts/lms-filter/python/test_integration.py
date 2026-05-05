from __future__ import annotations

from pathlib import Path

from lms_demo import DEFAULT_LIB_PATH, run_demo


def main() -> None:
    out_dir = Path(__file__).resolve().parent / "out" / "test"
    metrics = run_demo(
        lib_path=DEFAULT_LIB_PATH,
        output_dir=out_dir,
        fs=16000,
        duration_s=3.0,
        sine_hz=440.0,
        seed=11,
        reference_gain=1.0,
        save_wav=False,
    )
    if metrics["mse_cleaned"] >= metrics["mse_noisy"]:
        raise SystemExit(
            f"Expected LMS to improve MSE, got noisy={metrics['mse_noisy']} "
            f"cleaned={metrics['mse_cleaned']}"
        )
    if metrics["snr_improvement_db"] <= 0.5:
        raise SystemExit(f"Expected at least 0.5 dB SNR improvement, got {metrics['snr_improvement_db']}")
    print("python integration: ok")


if __name__ == "__main__":
    main()

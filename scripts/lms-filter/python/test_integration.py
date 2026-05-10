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
        seed=11,
        reference_gain=1.0,
        save_wav=False,
    )
    if metrics["error_tail_mse"] >= metrics["primary_tail_mse"]:
        raise SystemExit(
            f"Expected FXLMS residual tail MSE to improve, got "
            f"primary={metrics['primary_tail_mse']} error={metrics['error_tail_mse']}"
        )
    if metrics["attenuation_tail_db"] <= 0.5:
        raise SystemExit(
            f"Expected at least 0.5 dB tail attenuation, got {metrics['attenuation_tail_db']}"
        )
    print("python integration: ok")


if __name__ == "__main__":
    main()

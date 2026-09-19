"""Compare ESP32 per-sample scores against PC int8 per-sample scores (new dataset).

Reads the per-variant CSVs written by ml/validate_esp_uart.py --per-sample-csv and the
PC per-sample CSVs from the reproduction eval (results-new/). Reports max |score delta|,
mean delta, and prediction flips per variant. Target (as in milestone M6): max delta
<= 1 LSB of the model output (1/256 = 0.00390625) and zero flips.
"""

from __future__ import annotations

import argparse
from pathlib import Path

import pandas as pd

RETRAINING_ROOT = Path(__file__).resolve().parent
RESULTS_NEW = RETRAINING_ROOT / "retrain-waveform2" / "results-new"
ESP_DIR = RETRAINING_ROOT / "esp32-validation"

MAIN_VARIANTS = ["clean", "noise_snr30db", "noise_snr20db", "noise_snr10db", "noise_snr5db"]
LOWSNR_VARIANTS = ["noise_snr0db", "noise_snr-5db"]
ONE_LSB = 1.0 / 256.0


def load_pc(train_seed: int, noise_seed: int) -> pd.DataFrame:
    main = pd.read_csv(RESULTS_NEW / f"per_sample_train{train_seed}_noise{noise_seed}.csv")
    low = pd.read_csv(RESULTS_NEW / f"lowsnr_per_sample_train{train_seed}_noise{noise_seed}.csv")
    pc = pd.concat([main, low])
    return pc[pc["mode"] == "PC int8"]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--train-seed", type=int, default=44)
    parser.add_argument("--noise-seed", type=int, default=42, help="Noise seed of the features sent to the board.")
    parser.add_argument("--esp-dir", type=Path, default=ESP_DIR)
    args = parser.parse_args()

    pc = load_pc(args.train_seed, args.noise_seed)
    print(f"PC reference: train seed {args.train_seed}, noise seed {args.noise_seed}")
    print(f"{'variant':16s} {'n':>4s} {'max|d|':>9s} {'mean|d|':>9s} {'flips':>5s}  verdict")
    worst = 0.0
    total_flips = 0
    for variant in MAIN_VARIANTS + LOWSNR_VARIANTS:
        esp_path = args.esp_dir / f"esp32_{variant}_test.csv"
        if not esp_path.exists():
            print(f"{variant:16s}    - missing ({esp_path.name})")
            continue
        esp = pd.read_csv(esp_path).set_index("recording_id")
        ref = pc[pc["variant"] == variant].set_index("recording_id")
        common = ref.index.intersection(esp.index)
        if len(common) == 0:
            print(f"{variant:16s}    - no overlapping recordings")
            continue
        delta = (ref.loc[common, "score"] - esp.loc[common, "score"]).abs()
        flips = int((ref.loc[common, "pred"] != esp.loc[common, "pred"]).sum())
        worst = max(worst, float(delta.max()))
        total_flips += flips
        ok = "OK" if delta.max() <= ONE_LSB + 1e-9 and flips == 0 else "CHECK"
        print(f"{variant:16s} {len(common):4d} {delta.max():9.6f} {delta.mean():9.6f} {flips:5d}  {ok}")

    print(f"\n1 LSB = {ONE_LSB:.6f}. Overall: max|delta|={worst:.6f}, flips={total_flips} -> "
          f"{'PASS (<=1 LSB, 0 flips)' if worst <= ONE_LSB + 1e-9 and total_flips == 0 else 'INVESTIGATE'}")


if __name__ == "__main__":
    main()

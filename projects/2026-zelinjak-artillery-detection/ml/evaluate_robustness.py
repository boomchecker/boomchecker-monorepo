from __future__ import annotations

import argparse
import csv
from pathlib import Path

import numpy as np
import pandas as pd
from sklearn.metrics import accuracy_score, f1_score, matthews_corrcoef, precision_score, recall_score

from evaluate_pc import predict_keras, predict_tflite


PROJECT_ROOT = Path(__file__).resolve().parents[1]
FEATURE_ROOT = PROJECT_ROOT / "generated" / "features"
FEATURE_INDEX = FEATURE_ROOT / "features_manifest.csv"

# "clean" reported as a reference row alongside the paper's 30/20/10/5 dB sweep levels.
DEFAULT_SNR_DB_LEVELS = [30, 20, 10, 5]


def load_variant(feature_index: Path, variant: str) -> tuple[np.ndarray, np.ndarray, list[str]]:
    """Load every row for a given feature variant, across the full corpus (all splits)."""
    index = pd.read_csv(feature_index)
    rows = index[index["variant"] == variant]
    if rows.empty:
        raise SystemExit(f"No rows found for variant '{variant}' in {feature_index}")
    x = []
    y = []
    ids = []
    for row in rows.itertuples(index=False):
        mfcc = np.load(FEATURE_ROOT / row.feature_path)
        x.append(np.expand_dims(mfcc, axis=-1))
        y.append(int(row.class_id))
        ids.append(row.recording_id)
    return np.array(x), np.array(y), ids


def compute_metrics(y_true: np.ndarray, y_pred: np.ndarray) -> dict[str, float]:
    return {
        "accuracy": accuracy_score(y_true, y_pred),
        "precision": precision_score(y_true, y_pred, zero_division=0),
        "recall": recall_score(y_true, y_pred, zero_division=0),
        "f1": f1_score(y_true, y_pred, zero_division=0),
        "mcc": matthews_corrcoef(y_true, y_pred),
    }


def run_regime(model_path: Path, model_format: str, variant: str, feature_index: Path) -> dict[str, float]:
    x, y_true, _ = load_variant(feature_index, variant)
    if model_format == "keras":
        y_prob = predict_keras(model_path, x)
    else:
        y_prob = predict_tflite(model_path, x)
    y_pred = (y_prob >= 0.5).astype(int)
    metrics = compute_metrics(y_true, y_pred)
    metrics["samples"] = len(y_true)
    return metrics


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Waveform-domain SNR robustness sweep (Table II/III style) over the full corpus."
    )
    parser.add_argument("--model-keras", type=Path, help="Float32 Keras .h5 model (desktop float32 regime).")
    parser.add_argument("--model-tflite", type=Path, help="Quantized int8 TFLite model (PC int8 regime).")
    parser.add_argument("--features", type=Path, default=FEATURE_INDEX)
    parser.add_argument("--snr-db", type=float, nargs="+", default=DEFAULT_SNR_DB_LEVELS)
    parser.add_argument("--include-clean", action="store_true", help="Also report the noise-free 'clean' variant.")
    parser.add_argument("--output-csv", type=Path, default=None)
    args = parser.parse_args()

    if not args.model_keras and not args.model_tflite:
        raise SystemExit("Provide at least one of --model-keras / --model-tflite.")

    variants = []
    if args.include_clean:
        variants.append(("clean", "clean"))
    for snr_db in args.snr_db:
        variant_name = f"noise_snr{int(round(snr_db))}db"
        variants.append((f"{snr_db:g} dB", variant_name))

    rows: list[dict[str, str | int | float]] = []
    for label, variant in variants:
        if args.model_keras:
            metrics = run_regime(args.model_keras, "keras", variant, args.features)
            rows.append({"mode": "PC float32", "snr_db": label, **metrics})
        if args.model_tflite:
            metrics = run_regime(args.model_tflite, "tflite", variant, args.features)
            rows.append({"mode": "PC int8", "snr_db": label, **metrics})

    print(f"{'Mode':<12} {'SNR':>8} {'Samples':>8} {'Accuracy':>9} {'Precision':>10} {'Recall':>8} {'F1':>7} {'MCC':>7}")
    for row in rows:
        print(
            f"{row['mode']:<12} {row['snr_db']:>8} {row['samples']:>8} "
            f"{row['accuracy'] * 100:>8.2f}% {row['precision']:>10.2f} {row['recall']:>8.2f} "
            f"{row['f1']:>7.2f} {row['mcc']:>7.2f}"
        )

    if args.output_csv:
        args.output_csv.parent.mkdir(parents=True, exist_ok=True)
        with args.output_csv.open("w", newline="", encoding="utf-8") as handle:
            writer = csv.DictWriter(handle, fieldnames=list(rows[0].keys()))
            writer.writeheader()
            writer.writerows(rows)
        print(f"Wrote {args.output_csv}")


if __name__ == "__main__":
    main()

from __future__ import annotations

import argparse
import csv
from pathlib import Path

import numpy as np
import pandas as pd
import tensorflow as tf
from sklearn.metrics import accuracy_score, f1_score, matthews_corrcoef, precision_score, recall_score


PROJECT_ROOT = Path(__file__).resolve().parents[1]
FEATURE_INDEX = PROJECT_ROOT / "generated" / "features_seed42" / "features_manifest.csv"
DEFAULT_MODEL = PROJECT_ROOT / "archive" / "models" / "model.tflite"

DEFAULT_SNR_DB_LEVELS = [30, 20, 10, 5]


def load_variant(feature_index: Path, variant: str) -> tuple[np.ndarray, np.ndarray]:
    """Full-corpus load for one feature variant (M4 studies the full-corpus PC-int8 regime,
    matching how Table III was originally produced)."""
    index = pd.read_csv(feature_index)
    feature_root = feature_index.parent
    rows = index[index["variant"] == variant]
    x = []
    y = []
    for row in rows.itertuples(index=False):
        mfcc = np.load(feature_root / row.feature_path)
        x.append(np.expand_dims(mfcc, axis=-1))
        y.append(int(row.class_id))
    return np.array(x), np.array(y)


def predict_tflite_legacy_bug(model_path: Path, x: np.ndarray) -> np.ndarray:
    """Faithful port of ml/eval_tflite_pc.py:54-70, including the int8 output dequantization
    overflow bug at line 68: `(output_data[0][0] - zero_point) * scale` with both operands left
    as narrow numpy/Python int types. Under numpy 2.x (NEP 50), this stays in int8 arithmetic and
    wraps instead of promoting, e.g. 127 - (-128) wraps to -1 instead of 255. Everything else
    (input quantization, invocation) matches the fixed evaluate_pc.predict_tflite exactly, so the
    dequantization line is the only difference between this function and the corrected one.
    """
    interpreter = tf.lite.Interpreter(model_path=str(model_path))
    interpreter.allocate_tensors()
    input_details = interpreter.get_input_details()[0]
    output_details = interpreter.get_output_details()[0]
    predictions = []

    for sample in x:
        input_tensor = np.expand_dims(sample, axis=0).astype(np.float32)
        if input_details["dtype"] == np.int8:
            scale, zero_point = input_details["quantization"]
            quantized_tensor = np.round(input_tensor / scale + zero_point)
            input_tensor = np.clip(quantized_tensor, -128, 127).astype(np.int8)
        interpreter.set_tensor(input_details["index"], input_tensor)
        interpreter.invoke()
        output_data = interpreter.get_tensor(output_details["index"])
        if output_details["dtype"] == np.int8:
            scale, zero_point = output_details["quantization"]
            pred_prob = (output_data[0][0] - zero_point) * scale  # bug: narrow int8 subtraction
        else:
            pred_prob = output_data[0][0]
        predictions.append(float(pred_prob))
    return np.array(predictions)


def compute_metrics(y_true: np.ndarray, y_pred: np.ndarray) -> dict[str, float]:
    return {
        "accuracy": accuracy_score(y_true, y_pred),
        "precision": precision_score(y_true, y_pred, zero_division=0),
        "recall": recall_score(y_true, y_pred, zero_division=0),
        "f1": f1_score(y_true, y_pred, zero_division=0),
        "mcc": matthews_corrcoef(y_true, y_pred),
    }


def main() -> None:
    parser = argparse.ArgumentParser(
        description=(
            "M4: replicate ml/eval_tflite_pc.py's int8 dequantization overflow bug on the "
            "canonical archive/model.tflite over the full corpus, to test whether it explains "
            "the published Table III PC-int8 numbers."
        )
    )
    parser.add_argument("--model", type=Path, default=DEFAULT_MODEL)
    parser.add_argument("--features", type=Path, default=FEATURE_INDEX)
    parser.add_argument("--snr-db", type=float, nargs="+", default=DEFAULT_SNR_DB_LEVELS)
    parser.add_argument("--output-csv", type=Path, default=None)
    parser.add_argument("--per-sample-csv", type=Path, default=None)
    args = parser.parse_args()

    variants = [("clean", "clean")]
    for snr_db in args.snr_db:
        variants.append((f"{snr_db:g} dB", f"noise_snr{int(round(snr_db))}db"))

    rows: list[dict] = []
    per_sample_rows: list[dict] = []
    for label, variant in variants:
        x, y_true = load_variant(args.features, variant)
        y_prob = predict_tflite_legacy_bug(args.model, x)
        y_pred = (y_prob >= 0.5).astype(int)
        metrics = compute_metrics(y_true, y_pred)
        metrics["samples"] = len(y_true)
        rows.append({"mode": "PC int8 legacy-bug", "snr_db": label, **metrics})
        for class_id, score, pred in zip(y_true.tolist(), y_prob.tolist(), y_pred.tolist()):
            per_sample_rows.append({"variant": variant, "class_id": class_id, "score": score, "pred": pred})

    print(f"{'SNR':>8} {'Samples':>8} {'Accuracy':>9} {'Precision':>10} {'Recall':>8} {'F1':>7} {'MCC':>7}")
    for row in rows:
        print(
            f"{row['snr_db']:>8} {row['samples']:>8} "
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

    if args.per_sample_csv:
        args.per_sample_csv.parent.mkdir(parents=True, exist_ok=True)
        with args.per_sample_csv.open("w", newline="", encoding="utf-8") as handle:
            writer = csv.DictWriter(handle, fieldnames=list(per_sample_rows[0].keys()))
            writer.writeheader()
            writer.writerows(per_sample_rows)
        print(f"Wrote {args.per_sample_csv}")


if __name__ == "__main__":
    main()

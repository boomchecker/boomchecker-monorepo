from __future__ import annotations

import argparse
import csv
import re
import struct
import time
from pathlib import Path

import numpy as np
import pandas as pd
import serial
from sklearn.metrics import accuracy_score, f1_score, matthews_corrcoef, precision_score, recall_score


PROJECT_ROOT = Path(__file__).resolve().parents[1]
FEATURE_ROOT = PROJECT_ROOT / "generated" / "features"
FEATURE_INDEX = FEATURE_ROOT / "features_manifest.csv"

# Matches the board's "PREDIKCIA: 0.9961 (Cas: 32 ms)" response, see firmware/esp32s3/main/main.cpp.
RESPONSE_RE = re.compile(r"PREDIKCIA:\s*([-\d.]+)\s*\(Cas:\s*(\d+)\s*ms\)")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Stream MFCC features to an ESP32-S3 over UART and report accuracy/latency statistics."
    )
    parser.add_argument("--port", required=True)
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--split", default="test", help="Use 'all' to validate the full corpus regardless of split.")
    parser.add_argument("--variant", default="clean", help="Feature variant to send, e.g. clean or noise_snr30db.")
    parser.add_argument("--features", type=Path, default=FEATURE_INDEX)
    parser.add_argument("--limit", type=int, default=0)
    parser.add_argument("--timeout", type=float, default=3.0)
    parser.add_argument("--per-sample-csv", type=Path, default=None, help="Write one row per sample with the raw score.")
    args = parser.parse_args()

    index = pd.read_csv(args.features)
    rows = index[index["variant"] == args.variant]
    if args.split != "all":
        rows = rows[rows["split"] == args.split]
    if rows.empty:
        raise SystemExit(f"No rows found for variant='{args.variant}' split='{args.split}' in {args.features}")
    if args.limit:
        rows = rows.head(args.limit)

    y_true: list[int] = []
    y_pred: list[int] = []
    latencies_ms: list[int] = []
    errors = 0
    per_sample_rows: list[dict] = []

    with serial.Serial(args.port, args.baud, timeout=args.timeout) as uart:
        time.sleep(1.0)
        for row in rows.itertuples(index=False):
            mfcc = np.load(FEATURE_ROOT / row.feature_path).astype(np.float32).flatten()
            if len(mfcc) != 696:
                print(f"skip {row.recording_id}: expected 696 floats, got {len(mfcc)}")
                continue
            uart.write(struct.pack(f"<{len(mfcc)}f", *mfcc))
            uart.flush()
            response = uart.readline().decode("utf-8", errors="replace").strip()

            match = RESPONSE_RE.search(response)
            if not match:
                print(f"{row.recording_id}: no valid response ({response!r})")
                errors += 1
                continue

            score = float(match.group(1))
            latency_ms = int(match.group(2))
            # >=0.5, matching the PC-side threshold in evaluate_pc.py / evaluate_robustness.py
            # (the board's own C++ dequantization widens to int32 automatically, so no overflow
            # bug applies here - this only aligns the decision threshold, not the score itself).
            predicted = 1 if score >= 0.5 else 0

            y_true.append(int(row.class_id))
            y_pred.append(predicted)
            latencies_ms.append(latency_ms)
            per_sample_rows.append(
                {
                    "recording_id": row.recording_id,
                    "variant": args.variant,
                    "split": row.split,
                    "class_id": int(row.class_id),
                    "score": score,
                    "pred": predicted,
                    "latency_ms": latency_ms,
                }
            )
            print(f"{row.recording_id},expected={row.class_id},predicted={predicted},score={score:.4f},latency_ms={latency_ms}")

    if args.per_sample_csv:
        args.per_sample_csv.parent.mkdir(parents=True, exist_ok=True)
        with args.per_sample_csv.open("w", newline="", encoding="utf-8") as handle:
            writer = csv.DictWriter(handle, fieldnames=list(per_sample_rows[0].keys()))
            writer.writeheader()
            writer.writerows(per_sample_rows)
        print(f"Wrote {args.per_sample_csv}")

    if not y_true:
        print("No valid responses received; no statistics to report.")
        return

    y_true_arr = np.array(y_true)
    y_pred_arr = np.array(y_pred)
    print("\n=== ESP32-S3 UART validation results ===")
    print(f"Variant           : {args.variant}")
    print(f"Samples evaluated : {len(y_true_arr)} (errors/no-response: {errors})")
    print(f"Accuracy          : {accuracy_score(y_true_arr, y_pred_arr):.4f}")
    print(f"Precision         : {precision_score(y_true_arr, y_pred_arr, zero_division=0):.4f}")
    print(f"Recall            : {recall_score(y_true_arr, y_pred_arr, zero_division=0):.4f}")
    print(f"F1                : {f1_score(y_true_arr, y_pred_arr, zero_division=0):.4f}")
    print(f"MCC               : {matthews_corrcoef(y_true_arr, y_pred_arr):.4f}")
    print(f"Avg latency (ms)  : {sum(latencies_ms) / len(latencies_ms):.2f}")


if __name__ == "__main__":
    main()

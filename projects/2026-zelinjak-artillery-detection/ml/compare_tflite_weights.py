from __future__ import annotations

import argparse
import csv
from pathlib import Path

import numpy as np
import pandas as pd

from evaluate_pc import predict_tflite


PROJECT_ROOT = Path(__file__).resolve().parents[1]
FEATURE_ROOT = PROJECT_ROOT / "generated" / "features"
FEATURE_INDEX = FEATURE_ROOT / "features_manifest.csv"


def load_clean(feature_index: Path) -> tuple[np.ndarray, np.ndarray, list[str]]:
    index = pd.read_csv(feature_index)
    feature_root = feature_index.parent
    rows = index[index["variant"] == "clean"]
    x = []
    y = []
    ids = []
    for row in rows.itertuples(index=False):
        mfcc = np.load(feature_root / row.feature_path)
        x.append(np.expand_dims(mfcc, axis=-1))
        y.append(int(row.class_id))
        ids.append(row.recording_id)
    return np.array(x), np.array(y), ids


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Per-sample score comparison of two int8 TFLite models on the clean variant."
    )
    parser.add_argument("--model-a", type=Path, required=True)
    parser.add_argument("--model-b", type=Path, required=True)
    parser.add_argument("--features", type=Path, default=FEATURE_INDEX)
    parser.add_argument("--output-csv", type=Path, default=None)
    args = parser.parse_args()

    x, y_true, ids = load_clean(args.features)
    score_a = predict_tflite(args.model_a, x)
    score_b = predict_tflite(args.model_b, x)

    delta = np.abs(score_a - score_b)
    pred_a = (score_a >= 0.5).astype(int)
    pred_b = (score_b >= 0.5).astype(int)
    flips = pred_a != pred_b

    print(f"Samples          : {len(y_true)}")
    print(f"Model A          : {args.model_a}")
    print(f"Model B          : {args.model_b}")
    print(f"Max |delta|      : {delta.max():.6f}")
    print(f"Mean |delta|     : {delta.mean():.6f}")
    print(f"Delta == 0       : {(delta == 0).sum()} / {len(delta)}")
    print(f"Prediction flips : {flips.sum()} / {len(delta)}")

    if args.output_csv:
        args.output_csv.parent.mkdir(parents=True, exist_ok=True)
        with args.output_csv.open("w", newline="", encoding="utf-8") as handle:
            writer = csv.writer(handle)
            writer.writerow(["recording_id", "class_id", "score_a", "score_b", "delta", "pred_a", "pred_b", "flip"])
            for rid, cid, sa, sb, d, pa, pb, fl in zip(
                ids, y_true, score_a, score_b, delta, pred_a, pred_b, flips
            ):
                writer.writerow([rid, cid, sa, sb, d, pa, pb, int(fl)])
        print(f"Wrote {args.output_csv}")


if __name__ == "__main__":
    main()

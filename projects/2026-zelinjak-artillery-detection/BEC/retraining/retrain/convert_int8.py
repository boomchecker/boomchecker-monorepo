"""Full-integer int8 conversion for a retrained model.

Same converter settings as ml/convert_model.py (Optimize.DEFAULT, TFLITE_BUILTINS_INT8,
int8 input/output), but the representative dataset is drawn strictly from the retrain
TRAIN partition of splits3.csv — never from validation or test — and is read from the
feature index passed on the command line rather than a hard-coded feature root.
"""

from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
import pandas as pd
import tensorflow as tf

RETRAIN_ROOT = Path(__file__).resolve().parent
PROJECT_ROOT = RETRAIN_ROOT.parents[2]

REPRESENTATIVE_SAMPLES = 100


def representative_dataset(feature_index: Path, splits3: Path):
    feature_root = feature_index.parent
    index = pd.read_csv(feature_index)
    clean = index[index["variant"] == "clean"]
    splits = pd.read_csv(splits3)
    train_ids = set(splits[splits["split3"] == "train"]["recording_id"])
    rows = clean[clean["recording_id"].isin(train_ids)].sort_values("recording_id").head(REPRESENTATIVE_SAMPLES)
    for row in rows.itertuples(index=False):
        mfcc = np.load(feature_root / row.feature_path)
        yield [np.expand_dims(mfcc, axis=(0, -1)).astype(np.float32)]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--model", type=Path, required=True)
    parser.add_argument("--features", type=Path, default=PROJECT_ROOT / "generated/features_seed42/features_manifest.csv")
    parser.add_argument("--splits3", type=Path, default=RETRAIN_ROOT / "splits3.csv")
    parser.add_argument("--output", type=Path, default=None, help="Default: <model>.tflite")
    args = parser.parse_args()
    output = args.output or args.model.with_suffix(".tflite")

    model = tf.keras.models.load_model(args.model)
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.representative_dataset = lambda: representative_dataset(args.features, args.splits3)
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    converter.inference_input_type = tf.int8
    converter.inference_output_type = tf.int8

    tflite_model = converter.convert()
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(tflite_model)
    print(f"Wrote {output} ({len(tflite_model)} B)")


if __name__ == "__main__":
    main()

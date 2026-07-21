from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
import pandas as pd
import tensorflow as tf


PROJECT_ROOT = Path(__file__).resolve().parents[1]
FEATURE_ROOT = PROJECT_ROOT / "generated" / "features"
FEATURE_INDEX = FEATURE_ROOT / "features_manifest.csv"
MODEL_ROOT = PROJECT_ROOT / "generated" / "models"


def representative_dataset(feature_index: Path):
    index = pd.read_csv(feature_index)
    train_rows = index[(index["split"] == "train") & (index["variant"] == "clean")].head(100)
    for row in train_rows.itertuples(index=False):
        mfcc = np.load(FEATURE_ROOT / row.feature_path)
        yield [np.expand_dims(mfcc, axis=(0, -1)).astype(np.float32)]


def write_c_header(tflite_path: Path, header_path: Path, symbol: str) -> None:
    data = tflite_path.read_bytes()
    values = ", ".join(f"0x{byte:02x}" for byte in data)
    header_path.write_text(
        f"const unsigned char {symbol}[] = {{{values}}};\n"
        f"const unsigned int {symbol}_len = {len(data)};\n",
        encoding="utf-8",
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--model", type=Path, default=MODEL_ROOT / "baseline_cnn.h5")
    parser.add_argument("--features", type=Path, default=FEATURE_INDEX)
    parser.add_argument("--output", type=Path, default=MODEL_ROOT / "model.tflite")
    parser.add_argument("--header", type=Path, default=MODEL_ROOT / "model_data.h")
    parser.add_argument("--symbol", default="gunshot_model_int8_tflite")
    args = parser.parse_args()

    model = tf.keras.models.load_model(args.model)
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.representative_dataset = lambda: representative_dataset(args.features)
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    converter.inference_input_type = tf.int8
    converter.inference_output_type = tf.int8

    tflite_model = converter.convert()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(tflite_model)
    write_c_header(args.output, args.header, args.symbol)
    print(f"Wrote {args.output}")
    print(f"Wrote {args.header}")


if __name__ == "__main__":
    main()

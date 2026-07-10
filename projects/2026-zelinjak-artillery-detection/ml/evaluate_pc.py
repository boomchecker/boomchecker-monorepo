from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
import pandas as pd
import tensorflow as tf
from sklearn.metrics import accuracy_score, confusion_matrix, f1_score, matthews_corrcoef, precision_score, recall_score


PROJECT_ROOT = Path(__file__).resolve().parents[1]
FEATURE_ROOT = PROJECT_ROOT / "generated" / "features"
FEATURE_INDEX = FEATURE_ROOT / "features_manifest.csv"


def load_features(split: str) -> tuple[np.ndarray, np.ndarray]:
    index = pd.read_csv(FEATURE_INDEX)
    rows = index[(index["split"] == split) & (index["variant"] == "clean")]
    x = []
    y = []
    for row in rows.itertuples(index=False):
        mfcc = np.load(FEATURE_ROOT / row.feature_path)
        x.append(np.expand_dims(mfcc, axis=-1))
        y.append(int(row.class_id))
    return np.array(x), np.array(y)


def predict_keras(model_path: Path, x: np.ndarray) -> np.ndarray:
    model = tf.keras.models.load_model(model_path)
    return model.predict(x, verbose=0).flatten()


def predict_tflite(model_path: Path, x: np.ndarray) -> np.ndarray:
    interpreter = tf.lite.Interpreter(model_path=str(model_path))
    interpreter.allocate_tensors()
    input_details = interpreter.get_input_details()[0]
    output_details = interpreter.get_output_details()[0]
    predictions = []

    for sample in x:
        input_tensor = np.expand_dims(sample, axis=0).astype(np.float32)
        if input_details["dtype"] == np.int8:
            scale, zero_point = input_details["quantization"]
            input_tensor = np.clip(np.round(input_tensor / scale + zero_point), -128, 127).astype(np.int8)
        interpreter.set_tensor(input_details["index"], input_tensor)
        interpreter.invoke()
        output = interpreter.get_tensor(output_details["index"])
        if output_details["dtype"] == np.int8:
            scale, zero_point = output_details["quantization"]
            # Widen to avoid int8 overflow: output[0][0] and zero_point can differ by up to 255,
            # which wraps around under numpy's default int8 arithmetic (NEP 50 casting).
            predictions.append(float((int(output[0][0]) - int(zero_point)) * scale))
        else:
            predictions.append(float(output[0][0]))
    return np.array(predictions)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--model", type=Path, required=True)
    parser.add_argument("--format", choices=["keras", "tflite"], required=True)
    parser.add_argument("--split", default="test")
    args = parser.parse_args()

    x, y_true = load_features(args.split)
    y_prob = predict_keras(args.model, x) if args.format == "keras" else predict_tflite(args.model, x)
    y_pred = (y_prob >= 0.5).astype(int)

    print(f"Split      : {args.split}")
    print(f"Samples    : {len(y_true)}")
    print(f"Accuracy   : {accuracy_score(y_true, y_pred):.4f}")
    print(f"Precision  : {precision_score(y_true, y_pred, zero_division=0):.4f}")
    print(f"Recall     : {recall_score(y_true, y_pred, zero_division=0):.4f}")
    print(f"F1         : {f1_score(y_true, y_pred, zero_division=0):.4f}")
    print(f"MCC        : {matthews_corrcoef(y_true, y_pred):.4f}")
    print("Confusion matrix:")
    print(confusion_matrix(y_true, y_pred))


if __name__ == "__main__":
    main()

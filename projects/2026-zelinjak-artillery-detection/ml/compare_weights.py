from __future__ import annotations

import argparse
import csv
from pathlib import Path

import numpy as np
import tensorflow as tf


# Maps a TFLite tensor index to (Keras layer name, kind). Indices are specific to the
# gunshot LeNet-like architecture (conv2d, conv2d_1, dense, dense_1) as emitted by
# ml/convert_model.py; re-verify indices with `interpreter.get_tensor_details()` if the
# architecture changes.
CONV_DENSE_LAYERS = {
    "conv2d": "conv",
    "conv2d_1": "conv",
    "dense": "dense",
    "dense_1": "dense",
}


def find_kernel_tensors(interpreter: tf.lite.Interpreter) -> dict[str, int]:
    """Match TFLite weight tensors to Keras layer names by shape (ignoring axis order)."""
    keras_shapes = {
        "conv2d": (32, 3, 3, 1),
        "conv2d_1": (64, 3, 3, 32),
        "dense": (64, 832),
        "dense_1": (1, 64),
    }
    details = interpreter.get_tensor_details()
    found = {}
    for d in details:
        if d["dtype"] != np.int8:
            continue
        shape = tuple(d["shape"])
        for layer_name, target_shape in keras_shapes.items():
            if layer_name in found:
                continue
            if shape == target_shape:
                found[layer_name] = d["index"]
    return found


def dequantize(interpreter: tf.lite.Interpreter, index: int) -> np.ndarray:
    detail = {d["index"]: d for d in interpreter.get_tensor_details()}[index]
    raw = interpreter.get_tensor(index).astype(np.float32)
    q = detail["quantization_parameters"]
    scales = np.array(q["scales"])
    zero_points = np.array(q["zero_points"])
    axis = q["quantized_dimension"]
    if len(scales) > 1:
        shape = [1] * raw.ndim
        shape[axis] = -1
        return (raw - zero_points.reshape(shape)) * scales.reshape(shape)
    return (raw - zero_points[0]) * scales[0]


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Compare dequantized TFLite conv/dense kernels against a reference Keras float32 model."
    )
    parser.add_argument("--keras-model", type=Path, required=True)
    parser.add_argument("--tflite-model", type=Path, required=True)
    parser.add_argument("--output-csv", type=Path, default=None)
    args = parser.parse_args()

    keras_model = tf.keras.models.load_model(args.keras_model)
    keras_weights = {layer.name: layer.get_weights()[0] for layer in keras_model.layers if layer.get_weights()}

    interpreter = tf.lite.Interpreter(model_path=str(args.tflite_model))
    interpreter.allocate_tensors()
    tensor_index = find_kernel_tensors(interpreter)

    rows = []
    for layer_name, kind in CONV_DENSE_LAYERS.items():
        if layer_name not in tensor_index or layer_name not in keras_weights:
            continue
        deq = dequantize(interpreter, tensor_index[layer_name])
        deq_keras_order = np.transpose(deq, (1, 2, 3, 0)) if kind == "conv" else np.transpose(deq, (1, 0))
        kw = keras_weights[layer_name]
        diff = np.abs(deq_keras_order - kw)
        rows.append(
            {
                "layer": layer_name,
                "shape": str(kw.shape),
                "max_abs_diff": float(diff.max()),
                "mean_abs_diff": float(diff.mean()),
            }
        )
        print(f"{layer_name:12s} shape={str(kw.shape):>16} max_abs_diff={diff.max():.6f} mean_abs_diff={diff.mean():.6f}")

    if args.output_csv:
        args.output_csv.parent.mkdir(parents=True, exist_ok=True)
        with args.output_csv.open("w", newline="", encoding="utf-8") as handle:
            writer = csv.DictWriter(handle, fieldnames=["layer", "shape", "max_abs_diff", "mean_abs_diff"])
            writer.writeheader()
            writer.writerows(rows)
        print(f"Wrote {args.output_csv}")


if __name__ == "__main__":
    main()

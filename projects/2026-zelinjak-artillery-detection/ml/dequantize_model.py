from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
import tensorflow as tf

from model import build_baseline_cnn

INPUT_SHAPE = (58, 12, 1)

# Keras layer name -> (kernel shape as stored in TFLite, kind). Tensors are matched by
# shape, mirroring ml/compare_weights.py; bias tensors are matched to the kernel via the
# graph structure fallback below (shape alone is ambiguous: conv2d_1 and dense both have
# 64 output channels).
KERAS_TFLITE_SHAPES = {
    "conv2d": ((32, 3, 3, 1), "conv"),
    "conv2d_1": ((64, 3, 3, 32), "conv"),
    "dense": ((64, 832), "dense"),
    "dense_1": ((1, 64), "dense"),
}


def dequantize_tensor(interpreter: tf.lite.Interpreter, index: int) -> np.ndarray:
    detail = {d["index"]: d for d in interpreter.get_tensor_details()}[index]
    raw = interpreter.get_tensor(index).astype(np.float32)
    q = detail["quantization_parameters"]
    scales = np.asarray(q["scales"], dtype=np.float64)
    zero_points = np.asarray(q["zero_points"], dtype=np.float64)
    axis = q["quantized_dimension"]
    if scales.size > 1:
        shape = [1] * raw.ndim
        shape[axis] = -1
        return ((raw - zero_points.reshape(shape)) * scales.reshape(shape)).astype(np.float32)
    return ((raw - zero_points[0]) * scales[0]).astype(np.float32)


def find_kernel_and_bias_tensors(interpreter: tf.lite.Interpreter) -> dict[str, tuple[int, int]]:
    """Return {keras_layer_name: (kernel_index, bias_index)}.

    Kernels are matched by exact shape (unique per layer). The bias for each layer is the
    int32 tensor whose length equals the kernel's output-channel count and whose per-channel
    scale count matches; when two layers share an output width (conv2d_1 and dense, both 64),
    ties are broken by picking the bias whose scales equal input_scale * kernel_scales of
    that layer, which is exact for full-integer TFLite conversion.
    """
    details = interpreter.get_tensor_details()
    kernels: dict[str, dict] = {}
    for d in details:
        if d["dtype"] != np.int8:
            continue
        shape = tuple(int(s) for s in d["shape"])
        for layer_name, (target_shape, _) in KERAS_TFLITE_SHAPES.items():
            if layer_name not in kernels and shape == target_shape:
                kernels[layer_name] = d

    int32_biases = [
        d
        for d in details
        if d["dtype"] == np.int32
        and len(d["shape"]) == 1
        and len(d["quantization_parameters"]["scales"]) == int(d["shape"][0])
    ]

    # Op-level pairing: each CONV_2D / FULLY_CONNECTED op lists (input, kernel, bias) —
    # unavailable via the public Python API, so pair by scale identity instead.
    tensor_scale = {d["index"]: np.asarray(d["quantization_parameters"]["scales"]) for d in details}
    activation_scales = {
        d["index"]: d["quantization_parameters"]["scales"][0]
        for d in details
        if d["dtype"] == np.int8 and len(d["quantization_parameters"]["scales"]) == 1
    }

    result: dict[str, tuple[int, int]] = {}
    for layer_name, kernel_detail in kernels.items():
        out_channels = int(kernel_detail["shape"][0])
        kernel_scales = tensor_scale[kernel_detail["index"]]
        candidates = [b for b in int32_biases if int(b["shape"][0]) == out_channels]
        if len(candidates) > 1:
            filtered = []
            for b in candidates:
                bias_scales = tensor_scale[b["index"]]
                ratio = bias_scales / kernel_scales
                # bias_scale = input_scale * kernel_scale exactly, so the ratio must be a
                # constant equal to one of the activation scales in the graph.
                if np.allclose(ratio, ratio[0], rtol=1e-6) and any(
                    np.isclose(ratio[0], s, rtol=1e-6) for s in activation_scales.values()
                ):
                    filtered.append(b)
            candidates = filtered or candidates
        if len(candidates) != 1:
            raise RuntimeError(
                f"Cannot uniquely pair bias for layer {layer_name}: "
                f"{[c['index'] for c in candidates]} candidates"
            )
        result[layer_name] = (kernel_detail["index"], candidates[0]["index"])
        int32_biases = [b for b in int32_biases if b["index"] != candidates[0]["index"]]
    return result


def main() -> None:
    parser = argparse.ArgumentParser(
        description=(
            "Reconstruct a float32 Keras model from a full-integer int8 TFLite model by "
            "dequantizing its conv/dense kernels and biases. The result approximates the "
            "(lost) pre-quantization float32 weights to within one quantization step."
        )
    )
    parser.add_argument("--tflite-model", type=Path, default=Path("archive/models/model.tflite"))
    parser.add_argument("--output-h5", type=Path, required=True)
    args = parser.parse_args()

    interpreter = tf.lite.Interpreter(model_path=str(args.tflite_model))
    interpreter.allocate_tensors()
    pairs = find_kernel_and_bias_tensors(interpreter)

    model = build_baseline_cnn(INPUT_SHAPE)
    layers_with_weights = {layer.name: layer for layer in model.layers if layer.get_weights()}
    if set(layers_with_weights) != set(KERAS_TFLITE_SHAPES):
        raise RuntimeError(
            f"Fresh model layer names {sorted(layers_with_weights)} do not match "
            f"expected {sorted(KERAS_TFLITE_SHAPES)}"
        )

    for layer_name, (kernel_index, bias_index) in pairs.items():
        kind = KERAS_TFLITE_SHAPES[layer_name][1]
        kernel = dequantize_tensor(interpreter, kernel_index)
        kernel = np.transpose(kernel, (1, 2, 3, 0)) if kind == "conv" else np.transpose(kernel, (1, 0))
        bias = dequantize_tensor(interpreter, bias_index)
        layers_with_weights[layer_name].set_weights([kernel, bias])
        print(f"{layer_name:12s} kernel tensor {kernel_index} -> {kernel.shape}, bias tensor {bias_index} -> {bias.shape}")

    args.output_h5.parent.mkdir(parents=True, exist_ok=True)
    model.save(args.output_h5)
    print(f"Wrote {args.output_h5}")


if __name__ == "__main__":
    main()

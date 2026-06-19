# ML Workflow

This directory contains the machine-learning workflow for the artillery-shot detector:

1. normalize the original recording metadata,
2. generate MFCC feature matrices from WAV recordings,
3. train the baseline CNN,
4. convert the trained model to int8 TFLite,
5. validate the model on PC and optionally through ESP32-S3 UART.

Run the workflow from the project root with `task`:

```bash
task manifest
task features
task train
task convert
task validate:pc
```

Generated features, trained models, and reports are written under `generated/` and are ignored by Git.

## Main Scripts

Use these scripts for the reproducible project workflow:

- `generate_manifest.py` reads original Parquet libraries from `datasets/recordings/metadata/original/`, maps them into binary labels, and writes:
  - `datasets/recordings/manifest.csv`
  - `datasets/recordings/splits.csv`
- `prepare_features.py` reads `manifest.csv`, loads WAV files from `datasets/recordings/audio/`, computes MFCC matrices, and writes `.npy` features plus `generated/features/features_manifest.csv`.
- `train_model.py` trains the baseline CNN from generated features and writes `generated/models/baseline_cnn.h5`.
- `convert_model.py` converts the trained Keras model to an int8 TFLite model and C header:
  - `generated/models/model.tflite`
  - `generated/models/model_data.h`
- `evaluate_pc.py` evaluates either Keras or TFLite models on a manifest split.
- `validate_esp_uart.py` sends generated MFCC matrices to an ESP32-S3 over UART and prints board responses.

## Data Contract

The normalized manifest uses these labels:

- `dana_artillery`, class `1`: positive artillery/DANA recordings.
- `other_gunshot`, class `0`: non-DANA gunshots used as hard negatives.
- `impulse_noise`, class `0`: non-gunshot impulse/noise recordings.

The split is deterministic and stored in CSV so training and validation use the same sample partition across runs.

MFCC features are always shaped as `58 x 12`. The ESP32 UART validator flattens each matrix to `696` float32 values, matching the firmware payload size.

## Important Paths

All paths are relative to the project root:

- Input audio: `datasets/recordings/audio/*.wav`
- Original metadata: `datasets/recordings/metadata/original/*.parquet`
- Normalized manifest: `datasets/recordings/manifest.csv`
- Split file: `datasets/recordings/splits.csv`
- Generated features: `generated/features/`
- Generated models: `generated/models/`
- Reference archived models: `archive/models/`

## Legacy Scripts

Several files were imported from the original thesis source archive and are kept for traceability:

- `main_cnn.py`
- `main_preprocessing.py`
- `generate_with_noise.py`
- `evaluate_model.py`
- `eval_tflite_pc.py`
- `convert_to_tflite.py`
- `single_input.py`
- plotting and confusion-matrix helpers

Prefer the new scripts listed in `Main Scripts` for current work. The legacy scripts still contain useful experiment details, but many assume the old working directory layout and hardcoded folder names.

## Dependencies

Install the project dependencies from the project root:

```bash
python -m pip install -r requirements.txt
```

ESP32 validation additionally needs a connected board flashed with the firmware from `firmware/esp32s3`.

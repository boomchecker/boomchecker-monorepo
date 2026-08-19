# ML Workflow

This directory contains the machine-learning workflow for the artillery-shot detector:

1. normalize the original recording metadata,
2. generate MFCC feature matrices from WAV recordings (clean and waveform-domain SNR-noised),
3. train the baseline CNN,
4. convert the trained model to int8 TFLite,
5. validate the model on PC (float32/int8, clean and noise-robustness sweep) and optionally through ESP32-S3 UART.

Run the workflow from the project root with `task` (see `../setup.sh` / `task setup` for the project venv):

```bash
task setup
task manifest
task features
task train
task convert
task validate:pc

# Waveform-domain SNR robustness sweep (30/20/10/5 dB), matching the paper's Table II/III protocol:
task features:robustness
task robustness:pc -- --model-keras generated/models/baseline_cnn.h5 --model-tflite generated/models/model.tflite --include-clean
```

Generated features, trained models, and reports are written under `generated/` and are ignored by Git.

See `../REPRODUCTION_NOTES.md` for a detailed comparison of reproduced numbers against the published paper, and for known discrepancies between this repository and the paper text (dataset size, sample rate, dependency versions).

## Main Scripts

Use these scripts for the reproducible project workflow:

- `generate_manifest.py` reads original Parquet libraries from `datasets/recordings/metadata/original/`, maps them into binary labels, and writes `datasets/recordings/manifest.csv` with a stratified 80/20 train/test split (`random_state=42`, matching the paper's methodology and `main_cnn.py:20-24`), plus `datasets/recordings/splits.csv`.
- `model.py` defines the shared CNN architecture (`build_baseline_cnn`, 72,193 trainable parameters) used by both the reproducible pipeline and the legacy scripts.
- `utils.py` defines the shared MFCC extraction primitives (peak detection, 60 ms windowing, framing, mel filterbank) used by both `prepare_features.py` and the legacy scripts.
- `prepare_features.py` reads `manifest.csv`, loads WAV files from `datasets/recordings/audio/`, computes MFCC matrices, and writes `.npy` features plus `generated/features/features_manifest.csv`. With `--include-noisy`, it also generates one waveform-domain SNR-noised variant per `--snr-db` level (default `30 20 10 5`) over the full corpus, using the correct `10*log10(signal_power/noise_power)` formula.
- `train_model.py` trains the baseline CNN from generated features (train-only MFCC-domain Gaussian augmentation + class weighting) and writes `generated/models/baseline_cnn.h5`.
- `convert_model.py` converts the trained Keras model to an int8 TFLite model and C header:
  - `generated/models/model.tflite`
  - `generated/models/model_data.h`
- `evaluate_pc.py` evaluates either Keras or TFLite models on a manifest split (clean data only).
- `evaluate_robustness.py` runs the waveform-domain SNR sweep (Table II/III style) over the full corpus for float32 and/or int8 models, reusing `evaluate_pc.py`'s prediction/metric code.
- `validate_esp_uart.py` sends generated MFCC matrices (any feature variant/SNR level) to an ESP32-S3 over UART and reports accuracy/precision/recall/F1/MCC/average latency, in addition to per-sample responses.

## Data Contract

The normalized manifest uses these labels:

- `dana_artillery`, class `1`: positive artillery/DANA recordings.
- `other_gunshot`, class `0`: non-DANA gunshots used as hard negatives.
- `impulse_noise`, class `0`: non-gunshot impulse/noise recordings.

The current manifest contains 854 events (63/85/706); the published paper reports 706 (62/644) for the same corpus description. See `../REPRODUCTION_NOTES.md` for the analysis of this discrepancy.

The split is a stratified 80/20 train/test split (`sklearn.model_selection.train_test_split(..., random_state=42, stratify=class_id)`), stored in CSV so training and validation use the same partition across runs. There is no separate validation split: the test split is used as Keras' `validation_data` during training, matching the paper's methodology (`main_cnn.py:45`) even though this is a known limitation (see "Out of scope" in `../REPRODUCTION_NOTES.md`).

MFCC features are always shaped as `58 x 12`. The ESP32 UART validator flattens each matrix to `696` float32 values, matching the firmware payload size (`EXPECTED_BYTES 2784` in `firmware/esp32s3/main/main.cpp`).

## Important Paths

All paths are relative to the project root:

- Input audio: `datasets/recordings/audio/*.wav`
- Original metadata: `datasets/recordings/metadata/original/*.parquet`
- Normalized manifest: `datasets/recordings/manifest.csv`
- Split file: `datasets/recordings/splits.csv`
- Generated features: `generated/features/`
- Generated models: `generated/models/`
- Generated reports: `generated/reports/`
- Reference archived models: `archive/models/`

## Legacy Scripts

These files were imported from the original thesis source archive and are kept for traceability. Many assume the old working-directory layout and hardcoded folder names that no longer exist in this repository, so they are not directly runnable here:

- `main_cnn.py` - end-to-end thesis training script; the reference for the paper's 80/20 stratified split and class-weighting, now reflected in `generate_manifest.py`/`train_model.py`.
- `main_preprocessing.py` - batch MFCC extraction from a hardcoded `nongunshots/` folder.
- `generate_with_noise.py` - waveform-domain noise + MFCC extraction for a single hardcoded class folder, at one fixed noise level.
- `evaluate_model.py` - desktop Keras `.h5` evaluation over hardcoded `*_mfcc_audio_noise` folders.
- `eval_tflite_pc.py` - desktop int8 TFLite evaluation, same folder layout.
- `convert_to_tflite.py` - reconstructs a `.tflite` file from a `model_data.h` C header (reverse of `convert_model.py`).
- `single_input.py` - one-off MFCC visualization for a hardcoded `gunshot.wav`.
- `add_noise.py`, `load_data.py` - MFCC-domain augmentation and hardcoded-folder data loading used by `main_cnn.py`.
- `evaluation.py`, `my_confusion_matrix.py` - SNR-sweep evaluation helpers; inject noise into the MFCC domain (not waveform-domain), so they do not match the paper's evaluation protocol - see `evaluate_robustness.py` instead.
- `plot_signal_with_noise.py` - single-file waveform-domain SNR plotting script; the source of the correct SNR formula reused in `prepare_features.py`/`evaluate_robustness.py`.

Prefer the scripts listed in `Main Scripts` for current work.

## Dependencies

Install the project dependencies into a local venv from the project root:

```bash
task setup
# or directly:
bash ./setup.sh
```

This creates `venv/` (gitignored) and installs the pinned versions in `requirements.txt`. All `task` commands in `Taskfile.yml` already invoke `venv/bin/python`.

ESP32 validation additionally needs a connected board flashed with the firmware from `firmware/esp32s3` (see that directory's `README.md`); this is currently unverified in this environment (no ESP-IDF/hardware available), see `../REPRODUCTION_NOTES.md`.

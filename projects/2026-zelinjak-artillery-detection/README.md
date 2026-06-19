# 2026 Zelinjak Artillery Detection

Clean project workspace for Jakub Zelinjak's artillery-shot detection work.

The imported source archive is intentionally not mirrored one-to-one. This directory keeps the useful project assets in a reproducible layout:

- `datasets/recordings/audio/` contains the canonical WAV recording library.
- `datasets/recordings/metadata/original/` contains the original Parquet libraries exported by the gunshot tools.
- `datasets/recordings/manifest.csv` is the normalized training manifest generated from the original metadata.
- `ml/` contains the MFCC extraction, training, conversion, and validation workflow.
- `firmware/esp32s3/` contains the deployable ESP-IDF project for ESP32-S3.
- `archive/` contains selected historical model and figure artefacts from the thesis source archive.

Generated MFCC features, trained models, reports, and firmware build outputs are written under `generated/` or `firmware/esp32s3/build/` and are ignored by Git.

## Workflow

From this directory:

```bash
task manifest
task features
task train
task convert
task validate:pc
```

Firmware build:

```bash
task firmware:build
```

Flash and monitor require an explicit serial port:

```bash
task firmware:flash ESPPORT=COM3
task firmware:monitor ESPPORT=COM3
```

End-to-end ESP UART validation also requires a port:

```bash
task validate:esp ESPPORT=COM3
```

## Data Labels

The normalized manifest maps the original libraries into binary training labels:

- `dana_artillery` -> class `1`
- `other_gunshot` -> class `0`
- `impulse_noise` -> class `0`

The original Parquet files are preserved unchanged under `datasets/recordings/metadata/original/`.

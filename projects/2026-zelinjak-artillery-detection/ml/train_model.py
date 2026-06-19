from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
import pandas as pd

from model import build_baseline_cnn


PROJECT_ROOT = Path(__file__).resolve().parents[1]
FEATURE_ROOT = PROJECT_ROOT / "generated" / "features"
FEATURE_INDEX = FEATURE_ROOT / "features_manifest.csv"
MODEL_ROOT = PROJECT_ROOT / "generated" / "models"
INPUT_SHAPE = (58, 12, 1)
NOISE_LEVELS = [0.1, 0.2, 0.3, 0.5]


def add_noise_to_mfcc(mfcc: np.ndarray, noise_level: float) -> np.ndarray:
    signal_std = np.std(mfcc)
    noise = np.random.normal(0, signal_std * noise_level, mfcc.shape)
    return mfcc + noise


def load_split(index: pd.DataFrame, split: str) -> tuple[np.ndarray, np.ndarray]:
    rows = index[(index["split"] == split) & (index["variant"] == "clean")]
    features = []
    labels = []
    for row in rows.itertuples(index=False):
        features.append(np.load(FEATURE_ROOT / row.feature_path))
        labels.append(int(row.class_id))
    x = np.array(features)[..., np.newaxis]
    y = np.array(labels)
    return x, y


def augment_train_data(x_train: np.ndarray, y_train: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    aug_features = []
    aug_labels = []
    for index in range(len(x_train)):
        original = x_train[index, :, :, 0]
        aug_features.append(original)
        aug_labels.append(y_train[index])
        for level in NOISE_LEVELS:
            aug_features.append(add_noise_to_mfcc(original, level))
            aug_labels.append(y_train[index])
    return np.array(aug_features)[..., np.newaxis], np.array(aug_labels)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--features", type=Path, default=FEATURE_INDEX)
    parser.add_argument("--epochs", type=int, default=50)
    parser.add_argument("--batch-size", type=int, default=8)
    parser.add_argument("--output", type=Path, default=MODEL_ROOT / "baseline_cnn.h5")
    args = parser.parse_args()

    index = pd.read_csv(args.features)
    x_train_clean, y_train = load_split(index, "train")
    x_val, y_val = load_split(index, "val")
    x_train, y_train_aug = augment_train_data(x_train_clean, y_train)

    model = build_baseline_cnn(INPUT_SHAPE)
    class_weights = {0: 1.0, 1: 4.0}
    history = model.fit(
        x_train,
        y_train_aug,
        epochs=args.epochs,
        batch_size=args.batch_size,
        validation_data=(x_val, y_val),
        class_weight=class_weights,
        verbose=1,
    )

    args.output.parent.mkdir(parents=True, exist_ok=True)
    model.save(args.output)
    pd.DataFrame(history.history).to_csv(args.output.with_suffix(".history.csv"), index=False)
    print(f"Wrote {args.output}")


if __name__ == "__main__":
    main()

"""Retrain the thesis CNN with a proper, fully seeded protocol.

Differences from the original ml/main_cnn.py, each fixing a methodology defect:
- Hard-coded three-way split from splits3.csv (train/val/test) instead of an
  unreproducible train_test_split over os.listdir() ordering; the test set is never
  touched here.
- Full seeding (random, numpy, tensorflow + op determinism), so two runs with the same
  seed produce identical weights; --seed makes training-seed sensitivity measurable.
- Validation set is used for early stopping with best-weight restore; the original
  monitored the test set and saved the last epoch unconditionally.
- Output path includes the seed — no silent overwriting of a shared model file.

Kept identical to the thesis on purpose: architecture (build_baseline_cnn), MFCC-domain
Gaussian augmentation of the train set (NOISE_LEVELS, noise scaled by per-sample MFCC
std), class_weight {0: 1, 1: 4}, batch size 8, max 50 epochs, Adam + binary
cross-entropy. Features come from the canonical clean MFCC set (bit-identical across
noise seeds), so retraining needs no feature regeneration.
"""

from __future__ import annotations

import argparse
import csv
import random
import sys
from pathlib import Path

import numpy as np
import pandas as pd

RETRAIN_ROOT = Path(__file__).resolve().parent
PROJECT_ROOT = RETRAIN_ROOT.parents[2]
sys.path.insert(0, str(PROJECT_ROOT / "ml"))

NOISE_LEVELS = [0.1, 0.2, 0.3, 0.5]  # thesis ml/load_data.py values


def load_clean_features(feature_index: Path, splits3: Path) -> dict[str, tuple[np.ndarray, np.ndarray]]:
    feature_root = feature_index.parent
    index = pd.read_csv(feature_index)
    clean = index[index["variant"] == "clean"][["recording_id", "feature_path", "class_id"]]
    merged = clean.merge(pd.read_csv(splits3), on="recording_id", validate="one_to_one", suffixes=("", "_s3"))
    if (merged["class_id"] != merged["class_id_s3"]).any():
        raise RuntimeError("class_id mismatch between features manifest and splits3.csv")
    out = {}
    for split in ("train", "val"):
        rows = merged[merged["split3"] == split].sort_values("recording_id")
        x = np.array([np.expand_dims(np.load(feature_root / p), -1) for p in rows["feature_path"]])
        out[split] = (x, rows["class_id"].to_numpy())
    return out


def augment(x: np.ndarray, y: np.ndarray, rng: np.random.Generator) -> tuple[np.ndarray, np.ndarray]:
    """Thesis augmentation (ml/add_noise.py): original + one copy per noise level with
    Gaussian MFCC-domain noise scaled by the sample's own std — but with a seeded RNG."""
    features, labels = [], []
    for i in range(len(x)):
        original = x[i, :, :, 0]
        features.append(original)
        labels.append(y[i])
        signal_std = float(np.std(original))
        for level in NOISE_LEVELS:
            features.append(original + rng.normal(0.0, signal_std * level, original.shape))
            labels.append(y[i])
    return np.array(features)[..., np.newaxis], np.array(labels)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument(
        "--dataset",
        choices=["old", "new"],
        default="old",
        help="'old' = original 854-recording corpus; 'new' = new-campaign launches (BEC/new-dataset) with shared negatives.",
    )
    parser.add_argument("--features", type=Path, default=None, help="Override the per-dataset default feature index.")
    parser.add_argument("--splits3", type=Path, default=None, help="Override the per-dataset default splits file.")
    parser.add_argument("--epochs", type=int, default=50)
    parser.add_argument("--batch-size", type=int, default=8)
    parser.add_argument("--patience", type=int, default=10)
    parser.add_argument("--output", type=Path, default=None, help="Default: models/retrained[_new]_seed{seed}.h5")
    args = parser.parse_args()

    if args.dataset == "new":
        features = args.features or PROJECT_ROOT / "generated/features_new_seed42/features_manifest.csv"
        splits3 = args.splits3 or PROJECT_ROOT / "BEC/new-dataset/splits3_new.csv"
        output = args.output or RETRAIN_ROOT / "models" / f"retrained_new_seed{args.seed}.h5"
    else:
        features = args.features or PROJECT_ROOT / "generated/features_seed42/features_manifest.csv"
        splits3 = args.splits3 or RETRAIN_ROOT / "splits3.csv"
        output = args.output or RETRAIN_ROOT / "models" / f"retrained_seed{args.seed}.h5"
    args.features, args.splits3 = features, splits3

    random.seed(args.seed)
    np.random.seed(args.seed)
    import tensorflow as tf

    tf.random.set_seed(args.seed)
    tf.config.experimental.enable_op_determinism()

    from model import build_baseline_cnn

    data = load_clean_features(args.features, args.splits3)
    x_train, y_train = data["train"]
    x_val, y_val = data["val"]
    x_aug, y_aug = augment(x_train, y_train, np.random.default_rng(args.seed))
    print(f"train {len(x_train)} -> augmented {len(x_aug)}; val {len(x_val)}")

    model = build_baseline_cnn(x_train.shape[1:])
    early_stop = tf.keras.callbacks.EarlyStopping(
        monitor="val_loss", patience=args.patience, restore_best_weights=True, verbose=1
    )
    history = model.fit(
        x_aug,
        y_aug,
        epochs=args.epochs,
        batch_size=args.batch_size,
        validation_data=(x_val, y_val),
        class_weight={0: 1.0, 1: 4.0},
        callbacks=[early_stop],
        verbose=2,
    )

    output.parent.mkdir(parents=True, exist_ok=True)
    model.save(output)
    history_csv = output.with_suffix(".history.csv")
    with history_csv.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=["epoch", *history.history.keys()])
        writer.writeheader()
        for epoch in range(len(history.history["loss"])):
            writer.writerow({"epoch": epoch, **{k: v[epoch] for k, v in history.history.items()}})

    val_loss, val_acc = model.evaluate(x_val, y_val, verbose=0)
    print(f"Wrote {output}")
    print(f"Best-epoch val_loss={val_loss:.4f} val_accuracy={val_acc:.4f} (epochs run: {len(history.history['loss'])})")


if __name__ == "__main__":
    main()

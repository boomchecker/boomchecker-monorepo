"""Retrain the thesis CNN with waveform-domain noise augmentation ONLY (no MFCC jitter).

Third arm of the augmentation ablation:
- arm 1 (BEC/retraining/retrain):           clean + MFCC-domain jitter
- arm 2 (BEC/retraining/retrain-waveform):  clean + MFCC-domain jitter + waveform-noise variants
- arm 3 (this):                  clean + waveform-noise variants

Identical to arm 2 (same architecture, same hard-coded splits3.csv, same seeding, same
clean-validation early stopping, same waveform augmentation features) except the thesis
MFCC-domain jitter is removed, isolating whether the jitter contributes anything once
domain-matched waveform augmentation is present.

Waveform augmentation features (noise seed 142, disjoint from eval seeds 42-46):
    venv/bin/python ml/prepare_features.py --include-noisy --snr-db 30 20 10 5 \
        --seed 142 --output generated/features_trainaug_seed142

Only train-partition variants are read; validation stays clean and the test partition is
untouched, as in the other arms.
"""

from __future__ import annotations

import argparse
import csv
import random
import sys
from pathlib import Path

import numpy as np
import pandas as pd

WF2_ROOT = Path(__file__).resolve().parent
RETRAINING_ROOT = WF2_ROOT.parent
PROJECT_ROOT = WF2_ROOT.parents[2]
sys.path.insert(0, str(PROJECT_ROOT / "ml"))

WAVEFORM_VARIANTS = ["noise_snr30db", "noise_snr20db", "noise_snr10db", "noise_snr5db"]


def load_split_features(
    feature_index: Path, splits3: Path, split3: str, variants: list[str]
) -> tuple[np.ndarray, np.ndarray]:
    feature_root = feature_index.parent
    index = pd.read_csv(feature_index)
    rows = index[index["variant"].isin(variants)][["recording_id", "feature_path", "class_id", "variant"]]
    splits = pd.read_csv(splits3)
    ids = set(splits[splits["split3"] == split3]["recording_id"])
    rows = rows[rows["recording_id"].isin(ids)].sort_values(["variant", "recording_id"])
    x = np.array([np.expand_dims(np.load(feature_root / p), -1) for p in rows["feature_path"]])
    return x, rows["class_id"].to_numpy()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--features", type=Path, default=PROJECT_ROOT / "generated/features_seed42/features_manifest.csv")
    parser.add_argument(
        "--waveform-features",
        type=Path,
        default=PROJECT_ROOT / "generated/features_trainaug_seed142/features_manifest.csv",
        help="Feature index holding waveform-noise variants for train-set augmentation (noise seed disjoint from eval).",
    )
    parser.add_argument("--splits3", type=Path, default=RETRAINING_ROOT / "retrain" / "splits3.csv")
    parser.add_argument("--epochs", type=int, default=50)
    parser.add_argument("--batch-size", type=int, default=8)
    parser.add_argument("--patience", type=int, default=10)
    parser.add_argument("--output", type=Path, default=None, help="Default: models/retrained_wf2_seed{seed}.h5")
    args = parser.parse_args()
    output = args.output or WF2_ROOT / "models" / f"retrained_wf2_seed{args.seed}.h5"

    random.seed(args.seed)
    np.random.seed(args.seed)
    import tensorflow as tf

    tf.random.set_seed(args.seed)
    tf.config.experimental.enable_op_determinism()

    from model import build_baseline_cnn

    x_train, y_train = load_split_features(args.features, args.splits3, "train", ["clean"])
    x_val, y_val = load_split_features(args.features, args.splits3, "val", ["clean"])
    x_wf, y_wf = load_split_features(args.waveform_features, args.splits3, "train", WAVEFORM_VARIANTS)
    x_aug = np.concatenate([x_train, x_wf])
    y_aug = np.concatenate([y_train, y_wf])
    print(f"train {len(x_train)} clean + waveform-noise {len(x_wf)} = {len(x_aug)}; val {len(x_val)} (clean)")

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

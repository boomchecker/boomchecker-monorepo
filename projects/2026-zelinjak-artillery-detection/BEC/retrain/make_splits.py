"""Create a fixed three-way train/val/test split for the retraining experiment.

The canonical 80/20 split (datasets/recordings/splits.csv) is kept intact: its test
partition (171 events) stays the final test set, untouched by training and model
selection, so retrained-model numbers remain comparable with the archived-model
evaluation and the ESP32 hardware measurements. The validation set is carved out of
the canonical train partition (stratified by class, fixed seed) and is used only for
early stopping / checkpoint selection.

Output: BEC/retrain/splits3.csv (recording_id, class_id, split3) — committed to git so
the partition is hard-coded, not re-derived at training time.
"""

from __future__ import annotations

import argparse
from pathlib import Path

import pandas as pd
from sklearn.model_selection import train_test_split

RETRAIN_ROOT = Path(__file__).resolve().parent
PROJECT_ROOT = RETRAIN_ROOT.parents[1]

VAL_FRACTION = 0.2
SEED = 42


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--splits", type=Path, default=PROJECT_ROOT / "datasets/recordings/splits.csv")
    parser.add_argument("--manifest", type=Path, default=PROJECT_ROOT / "datasets/recordings/manifest.csv")
    parser.add_argument("--output", type=Path, default=RETRAIN_ROOT / "splits3.csv")
    args = parser.parse_args()

    splits = pd.read_csv(args.splits)
    manifest = pd.read_csv(args.manifest)[["recording_id", "class_id"]]
    merged = splits.merge(manifest, on="recording_id", validate="one_to_one")

    test = merged[merged["split"] == "test"].copy()
    trainval = merged[merged["split"] == "train"].copy()
    train, val = train_test_split(
        trainval, test_size=VAL_FRACTION, random_state=SEED, stratify=trainval["class_id"]
    )

    out = pd.concat(
        [
            train.assign(split3="train"),
            val.assign(split3="val"),
            test.assign(split3="test"),
        ]
    )[["recording_id", "class_id", "split3"]].sort_values("recording_id")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    out.to_csv(args.output, index=False)

    counts = out.groupby(["split3", "class_id"]).size().unstack(fill_value=0)
    print(counts)
    print(f"Wrote {args.output} ({len(out)} rows)")


if __name__ == "__main__":
    main()

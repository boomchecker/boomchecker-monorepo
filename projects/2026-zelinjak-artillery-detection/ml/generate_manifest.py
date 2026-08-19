from __future__ import annotations

import csv
from pathlib import Path
from typing import Any

import numpy as np
import pandas as pd
from sklearn.model_selection import train_test_split


PROJECT_ROOT = Path(__file__).resolve().parents[1]
AUDIO_ROOT = PROJECT_ROOT / "datasets" / "recordings" / "audio"
METADATA_ROOT = PROJECT_ROOT / "datasets" / "recordings" / "metadata" / "original"
MANIFEST_PATH = PROJECT_ROOT / "datasets" / "recordings" / "manifest.csv"
SPLIT_PATH = PROJECT_ROOT / "datasets" / "recordings" / "splits.csv"

SOURCE_MAP = {
    "241008.parquet": ("dana_artillery", 1),
    "241009.parquet": ("dana_artillery", 1),
    "Dopad2121.parquet": ("dana_artillery", 1),
    "gunshots_nonDana.parquet": ("other_gunshot", 0),
    "ImpulseEvents.parquet": ("impulse_noise", 0),
    "libdata_impulz_2.1.parquet": ("impulse_noise", 0),
}


def listify(value: Any) -> list[str]:
    if value is None:
        return []
    if isinstance(value, list):
        return [str(item) for item in value]
    if isinstance(value, tuple):
        return [str(item) for item in value]
    if isinstance(value, np.ndarray):
        return [str(item) for item in value.tolist()]
    return [str(value)]


def metadata_value(other_params: Any, key: str) -> str:
    if isinstance(other_params, dict):
        value = other_params.get(key, "")
        return "" if value is None else str(value)
    return ""


def assign_splits(rows: list[dict[str, str | int]]) -> None:
    """Stratified 80/20 train/test split, matching the paper's methodology (ml/main_cnn.py:20-24:
    train_test_split(..., test_size=0.2, random_state=42, stratify=y_clean))."""
    class_ids = [int(row["class_id"]) for row in rows]
    indices = list(range(len(rows)))
    train_idx, test_idx = train_test_split(indices, test_size=0.2, random_state=42, stratify=class_ids)
    train_set = set(train_idx)
    for i, row in enumerate(rows):
        row["split"] = "train" if i in train_set else "test"


def main() -> None:
    rows: list[dict[str, str | int]] = []
    seen_filenames: set[str] = set()
    missing_files: list[str] = []
    duplicate_files: list[str] = []

    for parquet_name, (label, class_id) in SOURCE_MAP.items():
        parquet_path = METADATA_ROOT / parquet_name
        if not parquet_path.exists():
            raise FileNotFoundError(parquet_path)

        df = pd.read_parquet(parquet_path)
        for _, record in df.iterrows():
            filenames = listify(record.get("filenames"))
            for filename in filenames:
                if not filename:
                    continue
                if filename in seen_filenames:
                    duplicate_files.append(filename)
                    continue
                audio_path = AUDIO_ROOT / filename
                if not audio_path.exists():
                    missing_files.append(filename)
                    continue

                original_id = str(record.get("id", "") or Path(filename).stem)
                recording_id = f"{Path(filename).stem}"
                other_params = record.get("other_params")

                rows.append(
                    {
                        "recording_id": recording_id,
                        "original_id": original_id,
                        "filename": filename,
                        "audio_path": f"audio/{filename}",
                        "label": label,
                        "class_id": class_id,
                        "split": "",
                        "source_library": parquet_name,
                        "source_label": str(record.get("label", "") or ""),
                        "samplerate": int(record.get("samplerate", 0) or 0),
                        "channels": int(record.get("channels", 0) or 0),
                        "date": metadata_value(other_params, "date"),
                        "guntype": metadata_value(other_params, "guntype"),
                        "caliber": metadata_value(other_params, "caliber"),
                        "distance": metadata_value(other_params, "distance"),
                        "angle": metadata_value(other_params, "angle"),
                        "suppressor": metadata_value(other_params, "suppressor"),
                        "window_size": metadata_value(other_params, "window_size"),
                        "impulse_position": metadata_value(other_params, "impulse_position"),
                    }
                )
                seen_filenames.add(filename)

    for audio_path in sorted(AUDIO_ROOT.glob("*.wav")):
        if audio_path.name in seen_filenames:
            continue
        if audio_path.name.startswith(("241008_", "241009_")):
            label = "dana_artillery"
            class_id = 1
        else:
            print(f"Leaving unlabelled audio out of manifest: {audio_path.name}")
            continue
        recording_id = audio_path.stem
        rows.append(
            {
                "recording_id": recording_id,
                "original_id": recording_id,
                "filename": audio_path.name,
                "audio_path": f"audio/{audio_path.name}",
                "label": label,
                "class_id": class_id,
                "split": "",
                "source_library": "unreferenced_audio",
                "source_label": "",
                "samplerate": 0,
                "channels": 0,
                "date": "",
                "guntype": "Dana",
                "caliber": "",
                "distance": "",
                "angle": "",
                "suppressor": "",
                "window_size": "",
                "impulse_position": "",
            }
        )
        seen_filenames.add(audio_path.name)

    assign_splits(rows)
    rows.sort(key=lambda item: (str(item["split"]), int(item["class_id"]), str(item["filename"])))
    fieldnames = list(rows[0].keys()) if rows else []

    MANIFEST_PATH.parent.mkdir(parents=True, exist_ok=True)
    with MANIFEST_PATH.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)

    with SPLIT_PATH.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=["recording_id", "split"])
        writer.writeheader()
        writer.writerows({"recording_id": row["recording_id"], "split": row["split"]} for row in rows)

    counts = pd.DataFrame(rows).groupby(["split", "label"]).size().reset_index(name="count")
    print(f"Wrote {MANIFEST_PATH}")
    print(f"Wrote {SPLIT_PATH}")
    print(counts.to_string(index=False))
    if missing_files:
        print(f"Skipped {len(missing_files)} metadata rows with missing audio files.")
    if duplicate_files:
        print(f"Skipped {len(duplicate_files)} duplicate filenames.")


if __name__ == "__main__":
    main()

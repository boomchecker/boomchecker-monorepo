"""Build the new-campaign dataset: extract zips, write manifest and event-level splits.

Each zip in this directory holds the four microphone channels of ONE artillery shot
(synchronously cropped, channel delays preserved). The zip basename is the event id;
inner WAV names are NOT unique across zips (shot numbers repeat between sessions), so
every recording id derives from the zip name: <zip_basename>_ch<N>.

Extraction: export/<zip_basename>/<original name>.wav (gitignored; zips stay the source
of truth).

Split protocol (fixes the cross-split leakage class found in the original corpus):
- Launch events are split at EVENT level (all channels of one shot share a split), the
  event list sorted and split with a fixed seed.
- Train events contribute all four channels (microphone-position diversity as natural
  augmentation); val/test events contribute channel 1 only, their remaining channels
  are excluded entirely, so val/test rows stay statistically independent events.
- Non-launch (false alarm) recordings are shared with the old dataset unchanged,
  including their split assignment from BEC/retraining/retrain/splits3.csv — negatives
  are identical in both datasets by design.
- Old-campaign dana_artillery recordings are NOT included: they may be re-cuts of the
  same physical shots as the new zips, and including both would recreate cross-split
  duplicate contamination.

Outputs (committed):
- manifest_new.csv  — prepare_features-compatible manifest (+ event_id, channel)
- splits3_new.csv   — recording_id,event_id,class_id,split3 for the training scripts
"""

from __future__ import annotations

import argparse
import re
import wave
import zipfile
from pathlib import Path

import pandas as pd
from sklearn.model_selection import train_test_split

NEW_DATASET_ROOT = Path(__file__).resolve().parent
PROJECT_ROOT = NEW_DATASET_ROOT.parents[1]
AUDIO_ROOT = PROJECT_ROOT / "datasets" / "recordings"

TEST_EVENT_FRACTION = 0.2
VAL_EVENT_FRACTION = 0.2  # of the remaining train+val pool
SEED = 42
EXPECTED_CHANNELS = 4
CHANNEL_RE = re.compile(r"_ch(\d+)\.wav$", re.IGNORECASE)


def extract_zips(export_root: Path) -> dict[str, list[tuple[int, Path]]]:
    """Extract every zip into export/<zip_basename>/; return {event_id: [(channel, wav_path)]}."""
    events: dict[str, list[tuple[int, Path]]] = {}
    for zip_path in sorted(NEW_DATASET_ROOT.glob("*.zip")):
        event_id = zip_path.stem
        target = export_root / event_id
        with zipfile.ZipFile(zip_path) as archive:
            wav_names = sorted(n for n in archive.namelist() if n.lower().endswith(".wav"))
            if len(wav_names) != EXPECTED_CHANNELS:
                print(f"WARNING {event_id}: {len(wav_names)} WAVs (expected {EXPECTED_CHANNELS})")
            target.mkdir(parents=True, exist_ok=True)
            channels = []
            for name in wav_names:
                match = CHANNEL_RE.search(name)
                if not match:
                    raise RuntimeError(f"{event_id}: cannot parse channel from '{name}'")
                out_path = target / Path(name).name
                if not out_path.exists():
                    with archive.open(name) as src:
                        out_path.write_bytes(src.read())
                with wave.open(str(out_path)) as w:
                    if w.getnchannels() != 1:
                        print(f"WARNING {out_path.name}: {w.getnchannels()} channels, expected mono")
                channels.append((int(match.group(1)), out_path))
            events[event_id] = sorted(channels)
    return events


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--old-manifest", type=Path, default=AUDIO_ROOT / "manifest.csv")
    parser.add_argument(
        "--old-splits3", type=Path, default=PROJECT_ROOT / "BEC" / "retraining" / "retrain" / "splits3.csv"
    )
    args = parser.parse_args()

    export_root = NEW_DATASET_ROOT / "export"
    events = extract_zips(export_root)
    print(f"extracted {len(events)} events into {export_root}")

    event_ids = sorted(events)
    trainval_events, test_events = train_test_split(event_ids, test_size=TEST_EVENT_FRACTION, random_state=SEED)
    train_events, val_events = train_test_split(
        sorted(trainval_events), test_size=VAL_EVENT_FRACTION, random_state=SEED
    )
    split_of_event = (
        {e: "train" for e in train_events} | {e: "val" for e in val_events} | {e: "test" for e in test_events}
    )

    rows = []
    for event_id in event_ids:
        split = split_of_event[event_id]
        channels = events[event_id] if split == "train" else events[event_id][:1]
        for channel, wav_path in channels:
            rel_audio = Path("..") / ".." / wav_path.relative_to(PROJECT_ROOT)
            rows.append(
                {
                    "recording_id": f"{event_id}_ch{channel}",
                    "event_id": event_id,
                    "channel": channel,
                    "audio_path": rel_audio.as_posix(),
                    "label": "dana_artillery",
                    "class_id": 1,
                    "split": split,
                }
            )

    old_manifest = pd.read_csv(args.old_manifest)
    old_splits = pd.read_csv(args.old_splits3)[["recording_id", "split3"]]
    negatives = old_manifest[old_manifest["class_id"] == 0].merge(old_splits, on="recording_id", validate="one_to_one")
    for r in negatives.itertuples(index=False):
        rows.append(
            {
                "recording_id": r.recording_id,
                "event_id": r.recording_id,
                "channel": 1,
                "audio_path": r.audio_path,
                "label": r.label,
                "class_id": 0,
                "split": r.split3,
            }
        )

    manifest = pd.DataFrame(rows)
    manifest_path = NEW_DATASET_ROOT / "manifest_new.csv"
    manifest.to_csv(manifest_path, index=False)

    splits3 = manifest[["recording_id", "event_id", "class_id", "split"]].rename(columns={"split": "split3"})
    splits3_path = NEW_DATASET_ROOT / "splits3_new.csv"
    splits3.to_csv(splits3_path, index=False)

    print(manifest.groupby(["class_id", "split"]).size().unstack(fill_value=0))
    print(
        f"launch events: {len(train_events)} train (x{EXPECTED_CHANNELS} ch) / "
        f"{len(val_events)} val / {len(test_events)} test (1 ch each)"
    )
    print(f"Wrote {manifest_path} ({len(manifest)} rows)")
    print(f"Wrote {splits3_path}")


if __name__ == "__main__":
    main()

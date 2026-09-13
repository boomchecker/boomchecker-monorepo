# New dataset — launch recordings from the 4-microphone campaign

Created 2026-08-20 for the BEC2026 paper 53 camera-ready.

## What is here

- **50 zips** = 50 shots (events). Each zip holds **4 mono WAVs** (48 kHz, 16 bit,
  1 s) — the four microphone channels of the same shot, synchronously cropped, with
  their mutual delays preserved according to impulse arrival. The zip name is the
  event id (`<session>_<time in session>s_shot_<number>.zip`); the WAV names inside
  (`..._ch1..4.wav`) are NOT unique across zips — recording ids always derive from
  the zip name.
- `build_dataset.py` — extracts the zips into the gitignored `export/<zip>/` and
  generates `manifest_new.csv` + `splits3_new.csv`. Run with `task dataset-new:build`
  (from `BEC/`).
- `manifest_new.csv` — manifest compatible with `ml/prepare_features.py`
  (+ `event_id`, `channel` columns).
- `splits3_new.csv` — split file for the training scripts (`--dataset new`).

## How the dataset is constructed (key decisions)

| Split | Launch events | Channels/event | Launch samples | Negatives (shared) |
|---|---|---|---|---|
| train | 32 | **4** | 128 | 506 |
| val | 8 | **1** (ch1) | 8 | 127 |
| test | 10 | **1** (ch1) | 10 | 158 |

1. **Event-level split** (seed 42): all channels of one shot belong to the same
   split — the 4 channels of an event are correlated, so a channel-level split would
   leak (the same class of error as the duplicated SoundBible recordings in the old
   corpus).
2. **Train uses all 4 channels** — microphone position/propagation-path diversity as
   natural augmentation.
3. **Val and test use only 1 channel per event** — both sets drive decisions (val:
   early stopping/model selection; test: reported numbers), so they need statistically
   independent samples. The remaining 3 channels of val/test events are excluded from
   the manifest entirely (54 WAVs; they stay on disk in `export/`).
4. **Negatives (false alarms) are identical to the old dataset**, including their
   split assignment from `BEC/retraining/retrain/splits3.csv` — keeps the two datasets
   comparable.
5. **Old `dana_artillery` recordings are NOT included** — they may be different cuts
   of the same physical shots; including both would recreate cross-split duplicates.

## How to train with it

```bash
# from the BEC/ directory
task dataset-new:features                 # training features (seed 42 + aug seed 142)
task retrain-wf2:train:all DATASET=new    # canonical arm 3 on the new dataset
task dataset-new:features:eval            # eval features, noise seeds 43-46 (multi-seed eval)
```

All three training arms (`retrain*/train.py`) accept `--dataset {old,new}`
(default `old` — backward compatible); models trained on the new dataset carry
`_new_` in their name (e.g. `retrained_wf2_new_seed42.h5`).

Note: `ml/reproduce.py` has the old dataset's feature roots hard-coded — multi-seed
evaluation of the new dataset currently runs per seed via
`ml/evaluate_robustness.py --features generated/features_new_seedN/features_manifest.csv`
(parametrizing reproduce.py is TBD).

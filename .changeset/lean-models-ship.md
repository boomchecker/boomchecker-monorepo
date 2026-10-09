---
"fw-common-boomdetect": minor
"fw-bom-stm32node": minor
---

gbt_m1 is the default model - a forest on the new layout 4, which adds the modulation spectrum of the 1-4 kHz envelope to the spectral statistics - and the image carries five models, the new ones trained with the node's own field recordings.

The public data sets alone did not carry over to the node's microphone, so the
new models also learned from up to 67 recordings made with it (84 min): a DJI
Phantom 4 and a Runner 250 up to 100 m, outdoor backgrounds, traffic and office
confusers.

**Layout 4** (`stats_spectral_mod`, `src/extractor_mod.c`, 79 values): a drone
hovering 60-90 m up has the RMS of the empty sky, but its rotors still modulate
the broadband noise at the blade-pass rate (170-185 Hz for the Phantom 4). The
pipeline keeps the 1-4 kHz envelope in a two-second ring (`src/envelope.c`), and
layout 4 is layout 2 plus ten numbers from the ring's modulation spectrum.

- `boomdetect_extract_fn` takes a side channel (`boomdetect_side_t`, the ring),
  `boomdetect_extractor_t` gains `env_required` and a `prepare` hook, and
  `boomdetect_t` grows by the 8 KB ring, fed only for an extractor that asks.
- A layout-4 window that closes before the ring is full carries no decision
  (`window.warming`): a `detect` run with gbt_m1 or mlp_m1 decides after 2.2 s.

| model | what | default thr | |
|---|---|---|---|
| `gbt_m1` | 200 trees on layout 4 | 3.211 | the default |
| `mlp_f2` | MLP 68-32-16-1 on layout 2 | 7.656 | the previous default, for rollback |
| `mlp_m1` | MLP 78-32-16-1 on layout 4 | 8.400 | gbt_m1's reach, but its 1 FA/h point moves with the training seed |
| `mlp_v6` | MLP on layout 1, public data | 3.0 (was 15.0) | recalibrated on the node's microphone |
| `svm_v3` | linear SVM on layout 1, public data | 0.5 | with mlp_v6, the anchor of the selftest and the parity harness |

`detect <sec> [squelch_milli] [thr_milli] [dbg] [rule]`:

- `sec` up to 86400; 0 runs until a key.
- The RMS gate defaults to 0.003 instead of 0.010: outdoors a drone at 20 m
  and beyond sat at 0.004-0.009.
- A K-of-N alarm prints `ALM t=<s>.<ms> <ON|OFF> hits=<k>/<n>` on each change.
- `rule` sets the alarm for one run: `<k>of<n>` (default `2of4`) or `mean<n>`,
  on while the mean of the last n decisions relative to the threshold is >= 0.
- `DETEND` adds `alarms=<n>` (OFF-to-ON transitions) and `first_drone=` /
  `first_alarm=`, seconds into the run of the first DRONE window and of the
  first alarm (`-` if never).

The out-of-fold numbers, the threshold ladders and the outdoor procedure are in
`docs/firmware/detection/field-manual.md`. The training package (`bdtrain`)
trains on the field sessions and judges them out of fold (`train --field --folds
--augment`), exports two-hidden-layer MLPs, forests and layout 4 to C (`export
--models MODEL=CNAME --keep RUN:MODEL=CNAME`), and `extractor_test` and
`model_parity_test` hold the C to it.

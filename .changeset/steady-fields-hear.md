---
"fw-common-boomdetect": minor
"fw-bom-stm32node": minor
---

Models trained on the second outdoor session: mlp_f2 is the new default, gbt_f2 beside it, and every earlier model stays in the image.

The 30 September recordings - DJI Phantom 4 at 10-70 m, Runner 250 at 20-40 m,
wind on the microphone - showed the previous default `mlp_f1` alarming on 2 of 16
and nothing beyond 10 m: outdoors 85-90 % of the energy sat below 300 Hz and the
drone at 10 m was three times quieter than the close takes it had learned from.
With those recordings, a day of office confusers (drill, shredder, fan, wind from
the fan, clapping, snapping, mouth buzz, keys, glass, speech) and the public sets
in training, and judged out of fold on all 55 field recordings:

- `mlp_f2` (68 -> 32 -> 16 -> 1, the first two-hidden-layer MLP) alarms on 25 of
  33 drone recordings - 10 of the 16 new ones - with one false alarm in 22 own
  negatives (mouth buzz) and none per hour on 11.6 h of held-out public
  negatives. Its shipped threshold is the 1 false-alarm-window-per-hour point
  (15.855); 7.66 is the usual 5-per-hour point (29 of 33, 4 of 22, 2.0 per hour).
- `gbt_f2` (200 trees, 5800 nodes) alarms on 23 of 33 and on none of the 22
  negatives, at 1.9 public false alarms per hour.
- Every model still gives nothing on the DJI at 50 m behind a tree crown and at
  70 m, where the band above 6 kHz is down at the background; synthetic distance
  augmentation (50-500 m, windier floor) and a higher weight on the field
  recordings did not move that at all.

## The models in the image

All ten are compiled in and switch at run time: `model` lists them (`*` marks
the active one, each with its default threshold), `model <name>` selects one.
The selection is not persisted - a reset returns to `mlp_f2` - but it survives
across separate `stm32node-cli` calls, because opening the serial port does not
reset the board. `stm32node-cli model [<name>]` does the same from the host.

| model | what | default thr (milli) | out of fold: DJI /14, Runner /19, own negatives /22 | use it for |
|---|---|---|---|---|
| `mlp_f2` (default) | MLP 68-32-16-1, 2026-10-01 | 15855 (1 FA/h) | 10, 15, 1 | the field test; 7660 is its 5 FA/h point |
| `gbt_f2` | 200 trees, 2026-10-01 | 3263 (5 FA/h) | 9, 14, 0 | the second opinion from another family: when both alarm, it is a drone |
| `mlp_f1` | MLP 68-16-1, 2026-09-26 | 8466 (5 FA/h) | 6, 10, 2 | the previous default, kept for comparison and rollback |
| `gbt_f1` | 120 trees, 2026-09-26 | 2646 (5 FA/h) | 5, 9, 0 | its forest counterpart |
| `mlp_v6` | MLP 51-32-1, public data only | 3000 (hand-set) | 7, 10, 5 | the pre-field baseline; 16.7 public false alarms per hour |
| `svm_v3` | linear SVM, public data only | 500 | - | reference only; fires on a quarter of negative windows |
| `mlp_l2`, `gbt_l2`, `gbt_reg_l2` | run `full`, public data only | 6072 / 4808 / 2275 | - | almost nothing at their thresholds on the node's microphone |
| `cnn_small` | CNN on the log-mel patch | 5127 | - | over the time budget (9.9 ms per window), reports overrun |

The numbers are recordings alarmed at the model's default threshold, squelch 3,
alarm 2 of 4, each recording judged by a model that never heard it (the
`fw_aug` models saw the 2026-09-30 takes for the first time here). The full
tables, per distance, are in `docs/firmware/detection/field-manual.md`.

## `detect`: every parameter

On the board's console `detect <sec> [squelch_milli] [thr_milli] [dbg]`; from
the host `stm32node-cli detect <sec> [--squelch N] [--thr N] [--dbg]`. The
arguments are positional on the console, so to give `thr` you give `squelch`
too (`detect 30 3 7660`); the host tool fills a skipped one with the default.

| parameter | range | default | meaning |
|---|---|---|---|
| `sec` | 1..86400, or 0 | - | how long to run; 0 runs until any key in the console (any byte), the host tool stops it with any key or Ctrl-C; every run ends with a `DETEND` line |
| `squelch_milli` | 0..1000 | 3 (= RMS 0.003) | the per-frame RMS gate, in 1/1000 of full scale: a frame quieter than this is dropped and the 14-frame window starts over; 0 disables the gate |
| `thr_milli` | -20000..20000 | the selected model's own (15855 for `mlp_f2`) | the decision threshold on the model's raw output (a logit for the MLPs and forests), in 1/1000; a window with `dec >= thr` is called DRONE; a value outside the range is rejected, not clamped |
| `dbg` | 0 or 1 | 0 | 1 prints one line per frame (RMS, timing) on top of the window lines |

What comes back: `LVL t=<s> rms=<+d.ddd>` about once a second (the input
level - compare it with the squelch), `DET t=<s> span=<frames> dec=<+d.ddd>
<DRONE|noise>` for every window that closed (`t` is when it closed, `span`
how many frames it covered, which varies because the gate resets it),
`ALM t=<s> on|off hits=<k>/4` when the alarm changes state, and the trailer
`DETEND windows=<n> drones=<n> alarms=<n> overrun=<0|1> err=<0|1>`.

The alarm is the K-of-N vote over the last four window decisions: on at 2 of
4, off below 1 (`DETECT_ALARM_N`, `DETECT_ALARM_K_ON`, `DETECT_ALARM_K_OFF` in
`fw/bom-stm32node/App/detect/detect_service.h`); changing it means a rebuild,
but a logged run's `DET` lines let you count the alarms any other rule would
have given before you do. While `detect` runs the board does not service the
radio, and it needs `micslot a` after every reset - the board boots on slot B.

## Making it more sensitive (or less)

Work from the recorded ladders, every step 30 s with the same drone manoeuvre
and the same sequence of confusers (speech, walking, a car, wind) without it:

1. **Lower the threshold.** `mlp_f2`: 15855 (1 FA/h: 10/14 DJI, 15/19 Runner,
   1/22 negatives, 0.0 public false alarms per hour) -> 7660 (5 FA/h: 11/14,
   18/19, 4/22 - drill, mouth buzz, shredder, fan motor - 2.0 per hour) -> 5570
   (20 FA/h: 12/14, 18/19, 5/22, 3.9 per hour). On the 30 September takes the
   step to 7660 adds the DJI at 40 m (48 % of windows) and 50 m (87 %) and the
   Runner at 40 m (3-10 %, just above the alarm rule); 30 m DJI and 70 m stay
   below either. `gbt_f2`: 4120 (1 FA/h: 6/14, 8/19, 0/22) -> 3263 (default) ->
   2460 (20 FA/h: 11/14, 17/19, 2/22). Pick the highest threshold at which the
   drone still gives at least a third of its windows and the lowest at which
   no confuser alarms.
2. **Lower the squelch** only when `LVL rms=` of the background sits below
   0.003: outdoors on 30 September it was 0.004-0.007 and every drone window
   passed, so the gate is not what limits the range now. At a quieter site
   `detect 30 2 15855` or `1`; `0` forms windows in silence too, which costs
   false alarms in the quiet.
3. **Switch the model**: `model gbt_f2` for the second opinion, `model mlp_f1`
   to compare with what was deployed before.
4. **Loosen the alarm rule** last - 1 of 4 or 2 of 8 - after counting what it
   would have done on the logs; it needs a rebuild.
5. Keep the microphone port unobstructed and pointed at the sky: the earlier
   playback test only detected with the port facing the source.

Record the audio alongside (`stm32node-cli record 1 120` into a folder per
scenario, a background take without the drone first - that is the one
recording the training set still lacks); `bdtrain score` replays it through
every model at home without flying again.

## In the training package

`bdtrain train` reads `--augment-profile near|far`, trains the `mlp2` family,
`bdtrain export` writes MLPs with two hidden layers (`MLP_HIDDEN2`, `mlp_w2`
as a matrix, `mlp_w3`/`MLP_B3`) and takes `--keep RUN:MODEL=CNAME` for models
the registry knows under another name. `analysis/2026-10-01-retrain/` in the
data root holds the import script, the distance ladder and the run evaluations.

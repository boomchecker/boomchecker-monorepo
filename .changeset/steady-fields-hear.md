---
"fw-common-boomdetect": minor
"fw-bom-stm32node": minor
---

Models trained on the outdoor sessions: mlp_f2 is the new default (shipped at its 5 FA/h point after the 2 October ladder), gbt_f3 beside it, and every earlier model stays in the image.

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
  negatives. It first shipped at the 1 false-alarm-window-per-hour point
  (15.855); it now ships at the usual 5-per-hour point, 7.656 (29 of 33, 4 of
  22 - drill, buzz, shredder, fan motor - 2.0 per hour), because of what the
  next session showed (below).
- `gbt_f2` (200 trees, 5800 nodes) alarms on 23 of 33 and on none of the 22
  negatives, at 1.9 public false alarms per hour.

The 2 October session - the DJI hovering straight overhead at 20, 30, 40, 60,
80 and 90 m plus a climb from 10 to 100 m, the first outdoor backgrounds
(3.5 min quiet, the building's AC exhaust, people) and 30 minutes of traffic
at the Dejvice roundabout - none of which the models had heard:

- mlp_f2's raw logit stays positive out to 90 m (median +5 to +6, 93 % of
  windows above 0) while the outdoor backgrounds sit around 0; the 15.855
  threshold cut everything past 30 m. At 7.656 it alarms at all six heights
  (20 m 58 %, 30 m 77 %, 40 m 23 %, 60 m 31 %, 80 m 9 %, 90 m 16 % of
  windows) and follows the climb to about 60-70 m, with no alarm on the 37
  minutes of outdoor background and traffic; traffic starts to fire below a
  threshold of 7 (24 per hour at 5.57).
- `gbt_f3` is the forest retrained with those recordings (67 field recordings,
  84 min, same recipe): judged out of fold it alarms on 16 of 21 DJI and 13 of
  19 Runner recordings - all six heights - and on none of 27 negatives, at
  0.9 public false alarms per hour. The MLP retrained the same way did not
  beat mlp_f2 (more false alarms on traffic), so mlp_f2 keeps its weights.
- Not yet checked on the board: this version was built and tested on the
  host only.
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
| `mlp_f2` (default) | MLP 68-32-16-1, 2026-10-01 | 7656 (5 FA/h; 15855 until 2026-10-02) | 11, 18, 4 | the field test: the longest reach, clean outdoors and in traffic |
| `gbt_f3` | 200 trees, 2026-10-02 | 3438 (5 FA/h) | 16/21, 13, 0/27 | the second opinion from another family, retrained with the 2026-10-02 takes: when both alarm, it is a drone |
| `gbt_f2` | 200 trees, 2026-10-01 | 3263 (5 FA/h) | 9, 14, 0 | gbt_f3's predecessor, kept for comparison |
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
| `thr_milli` | -20000..20000 | the selected model's own (7656 for `mlp_f2`) | the decision threshold on the model's raw output (a logit for the MLPs and forests), in 1/1000; a window with `dec >= thr` is called DRONE; a value outside the range is rejected, not clamped |
| `dbg` | 0 or 1 | 0 | 1 prints one line per frame (RMS, timing) on top of the window lines |
| `rule` | `<k>of<n>` or `mean<n>`, n 1..32 | `2of4` | the alarm rule for this run, see below |

What comes back: `LVL t=<s> rms=<+d.ddd>` about once a second (the input
level - compare it with the squelch), `DET t=<s> span=<frames> dec=<+d.ddd>
<DRONE|noise>` for every window that closed (`t` is when it closed, `span`
how many frames it covered, which varies because the gate resets it),
`ALM t=<s> on|off hits=<k>/4` when the alarm changes state, and the trailer
`DETEND windows=<n> drones=<n> alarms=<n> overrun=<0|1> err=<0|1>`.

The default alarm is the K-of-N vote over the last four window decisions: on
at 2 of 4, off below 1 (`DETECT_ALARM_N`, `DETECT_ALARM_K_ON`,
`DETECT_ALARM_K_OFF` in `fw/bom-stm32node/App/detect/detect_service.h`); the
fifth argument picks another rule for one run. While `detect` runs the board
does not service the radio, and it needs `micslot a` after every reset - the
board boots on slot B.

## The alarm rule is a `detect` argument now

`detect <sec> [squelch_milli] [thr_milli] [dbg] [rule]` - `stm32node-cli detect
<sec> --rule <rule>` - takes the alarm rule for the run, not persisted like the
rest of `detect`:

- `<k>of<n>`: the vote, ON at k DRONE windows of the last n, OFF below k-1 (at
  least 1). The default stays `2of4` (`DETECT_ALARM_*` in detect_service.h).
- `mean<n>`: soft integration - the mean of the last n decisions relative to
  the threshold, ON while it is >= 0, windows before the first counting as 0.
  One window well above the threshold carries a few weak ones; a run just
  below never alarms. The `ALM` line then reads `mean=<+d.ddd>/<n>`.
- n is 1..32. The same rules live in `boomdetect_alarm` (C) and
  `boomdetect_train.decision` (Python), with one test scenario in both;
  `bdtrain compare --rule mean4` judges a run under it.

Replayed on the out-of-fold decisions of every field recording at the 1 FA/h
threshold, `mean4` kept every detection of `mlp_f2` (10 of 14 DJI, 15 of 19
Runner) and dropped its one false alarm (the mouth buzz); at the 5 FA/h
default it trades two weak 30 September takes for one office confuser, and
with threshold 9000 it gives no false alarm on any recording so far. The
default was left at `2of4` so the board behaves as before until the field
confirms it.

## Making it more sensitive (or less)

Work from the recorded ladders, every step 30 s with the same drone manoeuvre
and the same sequence of confusers (speech, walking, a car, wind) without it:

1. **Move the threshold.** `mlp_f2`: 15855 (1 FA/h: 10/14 DJI, 15/19 Runner,
   1/22 negatives, 0.0 public false alarms per hour; on the 2 October ladder
   only 20 and 30 m) -> 7656 (5 FA/h, the default: 11/14, 18/19, 4/22 -
   drill, mouth buzz, shredder, fan motor - 2.0 per hour; all six heights to
   90 m, 0 alarms outdoors and in traffic) -> 5570 (20 FA/h: 12/14, 18/19,
   5/22, 3.9 per hour; traffic 24 alarms per hour - for range measurements
   only). 9000 with rule mean4 gave no false alarm on any recording so far
   (six of the seven 2 October takes, 10 of 16 of 30 September). `gbt_f3`:
   4200 (1 FA/h: 11/21, 7/19, 0/27) -> 3438 (default: 16/21, 13/19, 0/27) ->
   2520 (20 FA/h: 17/21, 15/19, 5/27). Pick the highest threshold at which the
   drone still gives at least a third of its windows and the lowest at which
   no confuser alarms.
2. **Lower the squelch** only when `LVL rms=` of the background sits below
   0.003: outdoors on 30 September it was 0.004-0.007 and every drone window
   passed, so the gate is not what limits the range now. At a quieter site
   `detect 30 2 7656` or `1`; `0` forms windows in silence too, which costs
   false alarms in the quiet.
3. **Switch the model**: `model gbt_f3` for the second opinion, `model mlp_f1`
   to compare with what was deployed before.
4. **Change the alarm rule** per run: `--rule mean4` to drop the buzz false
   alarm without losing a detection, `--rule 1of4` to loosen the vote. Count
   what another rule would have done on the logged `DET` lines first.
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

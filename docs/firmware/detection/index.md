# Acoustic detection

The node classifies short windows of microphone audio as drone or not. The
arithmetic lives in `fw/common/boomdetect`, which builds both for the STM32 and
natively for a host; the board-specific wiring is `fw/bom-stm32node/App/detect/`.

## The chain

```
mic (PDM)  →  pdm_pcm  →  48 kHz PCM  →  ÷3  →  16 kHz
                                                  │
                        frames of 1024, hop 512 ──┤
                                                  │  RMS gate per frame
                                     MFCC (13 coefficients per frame)
                                                  │
                      14 accepted frames → 52 features
                        [mean, std, dmean, cmax] × 13
                                                  │
                                            classifier → decision
                                                  │
                                      decision ≥ threshold → DRONE
```

A window is 14 frames at a hop of 512, so about 448 ms of audio. Frames overlap
by half, which matters more than it looks: the first frame after the input goes
quiet still contains half a frame of the loud audio before it, and passes the
gate.

`dmean` is the mean absolute frame-to-frame change, which is what carries
propeller modulation; `cmax` is max minus mean, a peakiness measure that does not
move with level. Feature 0 is the mean of MFCC coefficient 0, which tracks
absolute loudness — the MLP models skip it deliberately, to stay gain-invariant.

## Where the code lives

| | |
|---|---|
| `fw/common/boomdetect/src/boomdetect.c` | decimation, FIFO, framing, gate, windowing, the per-frame descriptor |
| `fw/common/boomdetect/src/boomdetect_mfcc_f32.c` | MFCC, with one deviation from CMSIS-DSP (below) |
| `fw/common/boomdetect/src/frame_scalars.c` | eight spectral scalars per frame from the magnitude spectrum |
| `fw/common/boomdetect/src/extractor_*.c` | the three feature layouts (below) and their registry |
| `fw/common/boomdetect/src/nn_infer.c` | float32 interpreter for the small CNNs |
| `fw/common/boomdetect/src/boomdetect_alarm.c` | K-of-N alarm with hysteresis over window verdicts |
| `fw/common/boomdetect/models/` | one translation unit per model, each exporting a `classifier_t` |
| `fw/common/boomdetect/training/` | the Python that trains, compares and exports the models (below) |
| `fw/bom-stm32node/App/detect/detect_service.c` | microphone, pacing, alarm, console output |

The split is the same one `fw/common/boomlink` has against `App/link/`, and for
the same reason: `Core/` is CubeMX's and cross-compiles only for the Cortex-M33,
so anything living there can never be run on a host or compared against
anything.

Feeding samples and doing work are separate calls (`boomdetect_push`,
`boomdetect_step`) because the firmware must not run two MFCCs in one iteration
of its superloop — doing so overran the 21.33 ms budget and starved the USB
stack. `step()` does at most one frame, by contract rather than by comment.

## Console commands

| command | what it does |
|---|---|
| `detect <sec> [squelch_milli] [thr_milli] [dbg]` | run for `<sec>` seconds, stream `LVL`/`DET` lines and `ALM` on alarm transitions, end with `DETEND` |
| `model [name]` | list the classifiers in this image, or select one |
| `micslot [a\|b]` | which microphone of the PDM pair is decoded |
| `micdiag` | probe the PDM data pins |
| `detselftest` | run a fixed synthetic signal through the chain, print raw float bits |

Omitting `thr_milli` uses the selected model's own default. That means the same
`detect 20 0` means different things depending on what `model` last selected —
deliberately, because a linear SVM's decisions live around ±3 while an MLP's are
unbounded logits, so one global default would make one of the families useless.
`model` prints the value it will use.

Neither `model` nor `micslot` is persisted; a reset returns to the defaults.

### The alarm

A window is 448 ms and one logit; whether a drone is present is a property of
seconds. Above the classifier sits a K-of-N rule with hysteresis
(`boomdetect_alarm.h`): the alarm turns ON when at least 2 of the last 4
classified windows were called `DRONE`, and OFF when fewer than 1 were. The
board prints `ALM t=<s>.<ms> ON|OFF hits=<k>/<n>` only on transitions, and
`DETEND` counts the OFF→ON transitions in `alarms=`. Squelched frames yield no
window and do not move the history. The constants are in `detect_service.h` and
pinned by the host tool's tests; the training package evaluates clip-level
verdicts with the same rule, so "alarm" means one thing on both sides.

## Feature layouts

Every accepted frame leaves a 41-float descriptor behind: the 13 MFCC
coefficients, the 20 log-mel energies the DCT was computed from, and eight
spectral scalars from the magnitude spectrum the MFCC destroys its input into
(power above 4 kHz, power 1–4 kHz, centroid, flatness, 85 % roll-off, crest, the
strength of the best harmonic comb between 60 and 400 Hz and its fundamental).
The MFCC block comes first, so the deployed layout and every checked-in fixture
are unchanged bit for bit.

| id | extractor | width | what |
|---|---|---|---|
| 1 | `stats` | 52 | `[mean, std, dmean, cmax] × 13` — what `mlp_v6` and `svm_v3` read |
| 2 | `stats_spectral` | 69 | layout 1, then mean and std of the eight scalars, then log-mel flux |
| 3 | `logmel` | 280 | the 14 × 20 log-mel patch minus its mean, frame-major — a CNN's input |

The scalars are the things an MFCC envelope smooths away and that separate a
rotor from a hum: a closed mouth has almost nothing above 4 kHz, a single stable
harmonic series, and a flat-ish spectrum only where it has energy at all. The
arithmetic is specified by `training/boomdetect_train/features.py`; the C is
held to it by `extractor_test` on the `detselftest` signal, with stated
tolerances and the two discrete values (roll-off bin, comb fundamental) compared
only on windows the fixture marks as numerically stable.

## Training package

`fw/common/boomdetect/training` is the other half of the detector: the Python
that produces the weight tables the C ships and judges them. It reads the
firmware's own `mfcc_tables.h`, so it cannot disagree with the board about a
filter edge, and its own tests hold it to the `detselftest` fixture stage by
stage.

```sh
cd fw/common/boomdetect/training
python -m venv .venv && .venv/bin/pip install -e .[dev,torch]
bdtrain manifest            # enumerate the datasets present -> manifest.parquet
bdtrain features            # run the front end once, cache per-frame descriptors
bdtrain baseline            # score mlp_v6 / svm_v3 on every suite
bdtrain train --name r1     # MLP, SVM, gradient-boosted trees, CNNs
bdtrain compare r1          # the comparison report
bdtrain export r1           # headers, translation units, parity vectors
```

Data lives outside the repository (`~/Documents/boomdetect-data`, or
`BOOMDETECT_DATA`). Suites: `val` (held-out clips of the training sources),
`halmstad` and `salford` (public, never trained on), `real_mic` (the node's own
recordings), `stress` (synthetic hums and whistles). The threshold a model is
compared and exported with is the lowest one that keeps false-alarm windows
under a budget per hour on the `val` negatives; it is then applied unchanged to
the unseen suites. `README.md` in that directory has the details.

## Swapping the classifier

Each model is its own translation unit exporting one `classifier_t`, listed in
`src/classifier_registry.c`. This works because the weight arrays in the
generated headers are `static const`, so identical symbol names have internal
linkage and several models coexist in one image.

An entry declares which slice of the feature vector it reads
(`feature_offset`, `n_features`), its own operating point, and a `layout_id`.
That last one guards what a feature count cannot: reorder the aggregate and the
count still matches while every weight lands on the wrong input. It is only as
strong as the discipline of bumping it, so treat it as a tripwire — the real
check is the parity fixture.

Adding a model is a new file under `models/`, one line in the registry, and one
declaration in `models/models.h` so the compiler checks the pair. `bdtrain
export` generates the file and its weight header; the registry lines stay a
hand edit, and `model_parity_test` fails for any exported model the registry
does not list.

The image currently carries six: the deployed `mlp_v6` and `svm_v3` on layout 1,
and the comparison set from training run r2 — `svm_l2`, `gbt_l2` (gradient
boosted trees) and `mlp_l2` on layout 2, `cnn_1d` on layout 3 — each with the
threshold that kept its false-alarm windows under 5 per hour on the validation
negatives. `model <name>` switches; the default stays `mlp_v6`.

A model needing a different feature *representation* — raw frames for a CNN, say
— adds a **feature extractor** rather than an edit to the pipeline.
`boomdetect_extractor_t` (`include/extractor.h`) declares a name, the
`layout_id` it produces and how wide it is, and `boomdetect_init()` checks the
model's `layout_id` against the configured extractor's rather than against a
constant. So a new representation is one file under `src/`, a new id, a line in
the extractor registry, and a model that declares the same id; nothing in
`boomdetect.c` changes.

There is still exactly one extractor, `stats`, which is the aggregation
described above. The seam was put in while there was one implementation
deliberately: it is cheap now and expensive once a second one is being wedged in.
`registry_test` drives a fake extractor with a foreign layout end to end, and
checks that a mismatched pair in either direction is refused — which is what a
feature *count* cannot catch, since reordering the statistics keeps the width.

## What is measured, and what is not

This section matters more than the rest of the page. The detector has been
carrying assumptions that looked like facts.

| | status |
|---|---|
| Which microphone of the PDM pair is populated | **measured**, one board. With the wrong slot the chain decodes the empty half and reports a flat zero — indistinguishable from a perfectly quiet detector. |
| Threshold 15.0 | **measured** against ambient office noise, three minutes, no drone. The offline +7.25 fired on 23 of 396 windows, peaking at 12.05. |
| Session-to-session variance | **measured, and larger than the change it justified**: one session peaked at 12.05, another at 2.77, same board and firmware. |
| C refactor did not move any number | **measured**, `detselftest` bit-identical across the whole move — captured on the pre-move firmware and re-run on the post-move build on the same board; both runs are recorded in the fixture's header |
| `-O2` does not move any number | **measured**, the `Shipped` preset builds without sanitizers at `-O2` and CI diffs its fixture output against the `Debug` build's |
| Host and target agree bit for bit | **measured, and they do NOT** — same C, same input, but MFCC coefficients differ by up to 1.8e-4 relative and decisions by 1.1e-6 (below) |
| Train/deploy skew | **expressible but still not measured**: the window policy is now configuration rather than compile-time constants, so both sides can be run; nobody has run them |
| C matches the Python it is trained with | **measured on the host**: the Python front end reproduces the `detselftest` MFCC and features to 1e-4 relative and the `mlp_v6` decisions to 1e-5 (`training/tests/test_parity_selftest.py`); the C extractors reproduce the Python layouts 2 and 3 (`extractor_test`); every registered model's C forward pass reproduces its Python one (`model_parity_test`) |
| New models generalise to unseen recordings | **measured, and they do not yet**: every family trained on the public sets scores ~0.99 window AUC on held-out clips of those sets and 0.7–0.94 on Halmstad; the shipped `mlp_v6` and a plain linear SVM sit at the top of that range. Same-microphone field recordings are the missing data, not another architecture |
| Detection of an actual drone on this hardware | **never tested** |

### Train/deploy skew

The training pipeline and the firmware do not window audio the same way:

| | training | firmware |
|---|---|---|
| windows | sliding, hop 7 frames | disjoint runs of 14 |
| continuity | 14 contiguous frames | the gate resets accumulation, so a window can span silence |
| gate | median RMS of the window ≥ 0.002 | each frame ≥ 0.010, five times stricter |
| 48 → 16 kHz | no counterpart | decimate by 3 |

None of that is wrong on its face; it is simply undescribed by whatever the
model was fitted to.

Three of those four rows are now settable rather than compiled in.
`boomdetect_config_t` carries `accum_frames`, `hop` and `gate`, and
`BOOMDETECT_GATE_WINDOW_MEDIAN` implements the training pipeline's policy: no
frame is rejected, and the window is kept or dropped as a whole on the median of
its frames' RMS. Leaving all three at 0 gives the firmware's behaviour, which is
what every checked-in fixture still reproduces.

So the gap can now be run from both ends on a host. It has not been: quantifying
it against the numbers the model was actually fitted to still needs the Python
reference, which is the next piece of work.

### Host and target are not bit-identical

Running the same fixture through `boomdetect_selftest_tool` on x86-64 and
through `detselftest` on the board gives the same input checksum, the same 42
frames and 3 windows, and different bits:

| | |
|---|---|
| worst MFCC coefficient | 1.8e-4 relative (2694 ULP), 1.6e-7 absolute |
| worst decision | 1.1e-6 relative; board −11.476006 / −9.829288 / −10.548382 against host −11.476009 / −9.829277 / −10.548393 |

Identical C, different instruction selection: the Cortex-M33 build takes
CMSIS-DSP's architecture-specific paths at `-O2`, the host the generic ones at
`-O1` under sanitizers. Neither compiler owes anyone a particular summation
order, and float32 addition is not associative.

Two consequences worth stating plainly. Bit-exactness is only ever a
same-binary, same-target claim, which is why there are two fixtures and each is
compared only against its own side. And the parity suite this package exists for
cannot be a bit comparison either; it has to carry a tolerance, and 1e-6 on the
decision is the scale this measurement suggests.

### The CMSIS-DSP deviation

`boomdetect_mfcc_f32()` conditions the FFT input by RMS where upstream uses the
absolute maximum, with a different quiet-frame guard. The scaling is undone
afterwards so it cancels in exact arithmetic, but it changes float32 rounding and
which frames are left untouched.

It arrived as an unrecorded hand-edit to a vendored copy of CMSIS-DSP: 466 of
the 467 files that copy was committed with were byte-identical to v1.15.0, and
this one was not. (The changeset's count of 470 is the same tree at the point it
was deleted, after a LICENSE, a README and a patch file were added to it.) **The
deployed model was selected against the values it produces**, so reverting to
stock upstream would silently move every coefficient. It is now our own source
file with the licence attribution attached, and upstream is fetched untouched.

## Checking a change did not move the numbers

The microphone never repeats an input, so two `detect` runs can never be
compared. The generator in `fw/common/boomdetect/src/boomdetect_selftest.c`
drives the chain from an integer LCG instead — the same input on every platform,
no flash cost — and prints every stage as raw IEEE-754 bit patterns rather than
decimals, so "unchanged" means unchanged.

Two consumers, two fixtures, each compared only against its own side (see the
host/target note above for why one fixture would not do):

```sh
# the host, no hardware; part of `task test`
cd fw/common/boomdetect && ctest --preset Debug -R selftest_host

# a connected board, against its own recorded capture
cd fw/common/boomdetect && task setup && task check-board
```

A mismatch means one of: the RMS deviation above was lost, `ARM_MATH_LOOPUNROLL`
was switched on (upstream defaults it on, this build does not), `-O2` did not
survive a build change, or the change moved the arithmetic. All four are
findings, and the middle two no longer need a board to notice: `LOOPUNROLL`
fails the configure, and the `Shipped` preset builds at `-O2` without sanitizers
so CI can diff its output against the `Debug` build's.

## Host tests

```sh
cd fw/common/boomdetect && task test
```

ASan and UBSan are on by default in the preset. The suite covers the MFCC front
end, the registry, the pipeline's edge cases, every model's trained weights, the
fixture, the three extractors against the Python specification, the alarm, the
CNN interpreter, and — through a stub registry linked in place of the real one
— that the model table really is replaceable. The harness itself is checked able
to fail, because a test framework that silently returns 0 is the failure mode
this repository has already hit once.

On Windows the host suite runs under MinGW-w64 (no sanitizers there; the
fixture is compared within `1e-5` because that compiler lands a few ULP from
the Linux capture — see the fixture header), and the Python side runs with
`pytest` in `training/`.

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
| `fw/common/boomdetect/src/boomdetect.c` | decimation, FIFO, framing, gate, windowing |
| `fw/common/boomdetect/src/boomdetect_mfcc_f32.c` | MFCC, with one deviation from CMSIS-DSP (below) |
| `fw/common/boomdetect/models/` | one translation unit per model, each exporting a `classifier_t` |
| `fw/bom-stm32node/App/detect/detect_service.c` | microphone, pacing, console output |

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
| `detect <sec> [squelch_milli] [thr_milli] [dbg]` | run for `<sec>` seconds, stream `LVL`/`DET` lines, end with `DETEND` |
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

Adding a model is a new file under `models/` plus one line in the registry. A
model needing a different feature *representation* — raw frames for a CNN, say —
would add an extractor and a new `layout_id` rather than change the struct.

## What is measured, and what is not

This section matters more than the rest of the page. The detector has been
carrying assumptions that looked like facts.

| | status |
|---|---|
| Which microphone of the PDM pair is populated | **measured**, one board. With the wrong slot the chain decodes the empty half and reports a flat zero — indistinguishable from a perfectly quiet detector. |
| Threshold 15.0 | **measured** against ambient office noise, three minutes, no drone. The offline +7.25 fired on 23 of 396 windows, peaking at 12.05. |
| Session-to-session variance | **measured, and larger than the change it justified**: one session peaked at 12.05, another at 2.77, same board and firmware. |
| C refactor did not move any number | **measured**, `detselftest` bit-identical across the whole move |
| Train/deploy skew | **described, not measured** (below) |
| C matches the Python the models were trained with | **not verified at all** |
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
model was fitted to. Quantifying it needs the Python reference alongside the C,
which is the next piece of work.

### The CMSIS-DSP deviation

`boomdetect_mfcc_f32()` conditions the FFT input by RMS where upstream uses the
absolute maximum, with a different quiet-frame guard. The scaling is undone
afterwards so it cancels in exact arithmetic, but it changes float32 rounding and
which frames are left untouched.

It arrived as an unrecorded hand-edit to a vendored copy of CMSIS-DSP: 466 of
that copy's 467 files were byte-identical to v1.15.0, and this one was not. **The
deployed model was selected against the values it produces**, so reverting to
stock upstream would silently move every coefficient. It is now our own source
file with the licence attribution attached, and upstream is fetched untouched.

## Checking a change did not move the numbers

The microphone never repeats an input, so two `detect` runs can never be
compared. `detselftest` drives the chain from an integer LCG — bit-identical on
any platform, no flash cost — and prints every stage as raw IEEE-754 bit
patterns rather than decimals, so "unchanged" means unchanged.

```sh
# against a connected board
fw/apps/stm32node-cli/.venv/bin/python \
  fw/common/boomdetect/tests/vectors/check_selftest.py
```

A mismatch means one of: the RMS deviation above was lost, `ARM_MATH_LOOPUNROLL`
was switched on (upstream defaults it on, this build does not), `-O2` did not
survive a build change, or the change moved the arithmetic. All four are
findings.

## Host tests

```sh
cd fw/common/boomdetect && task test
```

ASan and UBSan are on by default in the preset. The suite covers the registry
and the pipeline's edge cases; it does **not** yet compare anything against
Python, so green means self-consistent, not correct.

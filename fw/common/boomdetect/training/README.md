# boomdetect training

The side of the detector that produces the numbers the C library ships and
judges them. `fw/common/boomdetect` owns the arithmetic that runs on the board;
this package owns the Python that mirrors it stage by stage (and is tested
against it), the training of every model family the registry knows, the
evaluation that compares them on identical windows, and the exporters that
write the model headers, translation units and parity fixtures the C tests
consume.

## Setup

```sh
cd fw/common/boomdetect/training
python -m venv .venv
.venv/bin/pip install -e .[dev]                 # numpy, scipy, soundfile, scikit-learn, ...
.venv/bin/pip install torch --index-url https://download.pytorch.org/whl/cpu   # CNNs only
.venv/bin/pytest                                # no data needed
```

On Windows use `.venv\Scripts\python -m pytest`. Ruff settings are the sibling
packages': `ruff check .` must be clean.

## Data

Datasets live outside the repository, under `~/Documents/boomdetect-data` or
`$BOOMDETECT_DATA`:

```
raw/DroneAudioDataset/            git clone https://github.com/saraalemadi/DroneAudioDataset
raw/ESC-50/                       git clone https://github.com/karolpiczak/ESC-50
raw/hf-drone-audio-detection-samples/train-000NN-of-00039.parquet
                                  `bdtrain fetch` - all 39 shards, 8.3 GB, resumable
raw/stress/                       synthetic hums, whistles, fans: `stress.build_stress_clips()`
raw/field/<session>/Positive/<name>/   the node's own field recordings, see below
raw/field/<session>/Negative/<name>/
manifest.parquet                  one row per clip
cache/frames/<source>.npz         per-frame descriptors, computed once
runs/<name>/                      trained models, reports, exports
```

Halmstad, Salford and the node's own recordings are read from
`fw/bom-stmnode/drony/data` (untracked); `datasets/sources.py` says where
everything is expected and records each dataset's provenance.

The HuggingFace clips are numbered with one running index per class, so
neighbouring indices are neighbouring seconds of one recording: the split is
decided on blocks of consecutive indices (`sources.hf_group`), not clip by clip.

| suite | role | what it answers |
|---|---|---|
| `test` | a third of the training sources, held out of training, threshold selection and model choice alike | what the detector actually does on audio nothing was fitted to |
| `val` | a tenth, held out of training but used to pick the threshold and rank the families | did the model learn the training distribution (optimistic by construction) |
| `halmstad`, `salford` | public sets never trained on | does it generalise to other microphones and drones |
| `real_mic` | the node's own microphone: speaker playback and room backgrounds | does it survive our chain (small, and a speaker is not a rotor) |
| `field` | the node's own microphone outdoors: real drones and the sounds that fool the detector | the question the project exists for; judged out of fold when a run trains on it |
| `stress` | synthetic closed/open hums, whistles, pink noise, fans | the specific failure the deployed model had |

### Field recordings

`stm32node-cli record <sec> <count>` writes a folder of equal chunks and an
`index.csv`. Rename the folder after what is in it and sort it into a session:

```
raw/field/2026-09-25/Positive/dji_blizko/chunk-002.wav ... index.csv
raw/field/2026-09-25/Positive/runner_daleko/...
raw/field/2026-09-25/Negative/tep-cerpadlo-fel/...
raw/field/2026-09-23/Positive/runner_03_hover_2m.wav      a single WAV works too
```

The prefix of a positive names the drone (`dji_` -> `dji_phantom4`, `runner_`
-> `runner250`, see `datasets/field.py`); a negative's folder name is its
category. Recordings from firmware before the PDM warm-up fix start every
stream with a pop (DC step, clipped ~0.12 s, decayed by ~0.4 s): delete the
first chunk of every stream or leave it - a clip that starts at the stream's
first sample drops 0.5 s either way. A folder is one recording and one leakage group, whatever
chunks are missing from it. Then `bdtrain manifest` and `bdtrain features field`.

## Commands

```sh
bdtrain fetch [SHARD..]       # download the HuggingFace shards, resumably
bdtrain manifest              # enumerate every dataset present -> manifest.parquet
bdtrain features [SOURCE..]   # run the front end, fill the frame cache
bdtrain baseline              # score the shipped mlp_v6 / svm_v3 on every suite
bdtrain train --name r1       # every family x layout (see run.py DEFAULT_FAMILIES)
bdtrain train --name f1 --field --share field=0.25 drone_audio_dataset=0.15 --folds 4
                              # + the field recordings, sources weighted, 4 fold models
bdtrain train --name f1 --field --augment 4 [--augment-profile far] ...
                              # + 4 distance variants of every field clip (augment.py;
                              # near = 20-250 m, far = 50-500 m, quieter, windier floor)
bdtrain train --name f1_nr --field --field-exclude runner250 ...
                              # the same without one drone (or one negative category)
bdtrain train ... --seed 1 --aug-seed 1
                              # another draw of the same recipe (weights, early-stopping
                              # split, augmentation); 42 / 0 = the old runs
bdtrain features --spec all [SOURCE..] [--workers 12]
                              # the band spectrograms (dsp/spectro.py) for layouts 1xx-9xx,
                              # cache/spec/<front-end>/<source>.npz, float16
bdtrain compare r1            # the comparison report -> runs/r1/report.md
bdtrain compare r1 --rule mean4      # judged under another alarm rule (2of4 default, 1of4, mean8 ...)
bdtrain export r1 [--models m ...]   # headers + translation units + parity vectors
bdtrain export fw4_modb --models gbt_l4=gbt_m1 mlp2_l4=mlp_m1 --keep fw2_mlp2:mlp2_l2=mlp_f2
                              # export under a new C name; parity for registry models of
                              # older runs, under the name the registry knows them by
                              # (with --fa-per-hour 5 --squelch 0.003 this is the export
                              # behind today's registry)
bdtrain fixtures              # regenerate tests/vectors/extractor_expected.h,
                              # extractor_mod_expected.h and src/envelope_coefs.h
```

`bdtrain` is `python -m boomdetect_train`.

## How a model is judged

The clips are partitioned once, deterministically from the group key:

    train 57 % | val 10 % | test 33 %

Two thirds are available to build a model with; the remaining third judges it
and is touched by nothing else. The threshold is chosen on `val`, never on
`test`, as the lowest value keeping false-alarm windows under a budget per hour
of negative audio (5/h by default), and is then applied unchanged to every
other suite. Two views at that threshold:

* window level: true-positive rate over positive windows, false alarms per
  hour of negative audio, and the threshold-free window AUC;
* clip level: K-of-N alarm verdicts (2-of-4, release below 1, the board's
  rule) over the clips long enough for the rule — the half-second training
  clips are not, and are not counted there.

The report also lists which negative categories fire, per model.

The field recordings are too few for that partition, so they are judged by
folds instead. `--folds K` deals every
field recording into one of K folds (each drone and the negatives spread
evenly) and trains, next to the run's models, K copies each missing one fold;
`compare` scores every field recording with the copy that never heard it, at
the board's squelch and at 0.003, per drone and per recording. The folds are
dealt over all field recordings, including any a run leaves out, so runs that
differ only in `--field-exclude` are judged on the same folds.

`--share SOURCE=FRACTION` fixes how much of its class's weight a source
carries; without it the HuggingFace set is 97 % of every drone window and the
field recordings would be a rounding error. The classes are balanced by weight
at the same time.

## Layouts and families

| layout | extractor | width | families |
|---|---|---|---|
| 1 | `stats` | 52 | `mlp`, `mlp_reg`, `mlp2`, `svm`, `gbt`, `gbt_reg` |
| 2 | `stats_spectral` | 69 | the same six |
| 3 | `logmel` | 280 | `cnn_small`, `cnn_ds`, `cnn_1d`, `cnn_wide`, and the same six on the patch as a flat vector (`gbt_l3` ...) |
| 4 | `stats_spectral_mod` | 79 | the same six; layout 2 plus ten modulation features of the 1-4 kHz envelope over the last 2 s (`features.modulation_stats`) - the family behind `gbt_m1` / `mlp_m1` |
| 7 | `stats_spectral_mod4s` | 79 | the same six; layout 2 + the modulation features over a 4 s ring (offline only) |
| 8 | `stats_spectral_mod2s4s` | 89 | the same six; layout 2 + the 2 s and the 4 s sets (offline only) |
| 9 | `stats_spectral_mod2s8s` | 89 | the same six; layout 2 + the 2 s and an 8 s set (offline only) |
| 10 | `stats_spectral_modspec` | 329 | the same six; layout 4 + the modulation prominence spectrum 10-500 Hz (offline only) |

The band-spectrogram layouts (offline only, no C side; `features.spec_layout`) are
`base + k`, k the front-end in `dsp/spectro.SPEC_FE_NAMES` (mfe1k mfe2k mfe4k gs1k gs2k
gs4k = mel or gammatone, 64 bands, FFT 1024/2048/4096): 100 patch 14 x 64, 400 patch
31 x 64 (~1 s), 200 per-band stats, 300 layout 4 + per-band stats, 500 / 600 patch 14 /
31 + layout 4 (hybrid), 700 / 900 patch 14 / 31 + layout 8, 800 patch 14 + layout 10.
The patch and hybrid layouts take the `models/torchnets.py` families `cnn_m`, `crnn1d`,
`crnn2d`, `lstm20` (the TalTech BEC 2026 architectures), trained on the GPU when one is
there. A window whose FFT or patch would reach before the start of its clip is dropped
(`features.min_start_frame`): mirrored history was a class fingerprint on the 0.5 s
HuggingFace drone clips. The 2026-10-07/09 comparison of all of them is in the
data root, `analysis/2026-10-09-souhrn/vysledky.md`.

`mlp` is 32 hidden units, `mlp_reg` 16 with alpha 1e-2, `mlp2` two hidden layers
(32, 16) with the same alpha - the family behind `mlp_f2`. The C side takes an
MLP with one or two hidden layers (export.py writes either header format).

`features.py` is the specification of the four layouts; the C extractors are
held to it by `extractor_test`.

## Adding a model to the board

1. `bdtrain export RUN --models NAME` writes `models/<family>_model_data_NAME.h`,
   `models/model_NAME.c` and refreshes `tests/vectors/parity_vectors.h`.
2. Add the `extern const classifier_t classifier_NAME;` line to
   `models/models.h`, the `&classifier_NAME,` line to
   `src/classifier_registry.c`, and the translation unit to the `boomdetect`
   target in `CMakeLists.txt`. The parity test fails for any exported model
   the registry does not list, so this cannot be forgotten silently.
3. `task test` in `fw/common/boomdetect`, then the firmware build.

## What it cannot do yet

Make a model generalise to a microphone or a drone it has never heard: trained
on the public sets alone, every family scores about 0.99 window AUC on held-out
clips of those sets and 0.7–0.94 on Halmstad. The field recordings - two
drones, a few sites, 84 minutes - narrow that for this node; false alarms per
hour on real background still need hours of it.

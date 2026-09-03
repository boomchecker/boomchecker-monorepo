# Reproduction Notes

Findings from cleaning up and reproducing the results behind "A Lightweight and Robust Two-Stage
Acoustic Pipeline for Embedded Artillery Launch Detection" (BEC2026). This is the record for
reconciling the paper text with what the code and data in this repository actually produce, ahead
of addressing the review feedback. It was produced by running the pipeline directly (Python 3.11
venv, no Docker daemon or ESP-IDF available in that environment); firmware compilation and ESP32-S3
hardware numbers still need to be confirmed by whoever has a board and the `fw-devcontainer`.

**Update 2026-08-14:** the camera-ready reproduction effort (milestones M0-M6) is tracked in
`REPRODUCTION_ROADMAP.md`, which supersedes several "out of scope" items below (Section 9) **and
also supersedes the quantization-effect conclusion in Section 3 below** — see the correction note
inline there before reading Section 3's original numbers. The PC-int8 vs. ESP32-S3 discrepancy and
the "quantization improves robustness" claim now have concrete, evidence-backed explanations (not
just candidate suspects), and held-out results now exist. One-command harness: `task reproduce:pc`
(destructive reset: `task reproduce:clean` — this preserves the M6 hardware capture CSVs and the
two hand-authored `*.md` reports, but nothing else in `generated/`). Generated deliverables:
`generated/reports/reviewer_response.md`, `table2_float32.tex`, `table3_int8.tex`,
`weights_provenance.md`, `provenance_table3.md`.

## 1. Dataset size does not match the paper

The paper states 706 labeled events (62 artillery launches, 644 non-launch). The manifest generated
by `ml/generate_manifest.py` from the Parquet libraries actually checked into this repository
contains **854 events: 63 `dana_artillery` / 85 `other_gunshot` / 706 `impulse_noise`**.

Notably, the `impulse_noise` class alone equals the paper's entire corpus size. Decision (confirmed
with the author): **706 is treated as an inaccuracy/stale figure in the paper text**, not a target to
reconstruct. This repository's 854-event manifest is the reference going forward, and the paper's
Table I and the "706-event corpus" description need to be corrected in a future revision.

Practical effect: the robustness stress tests below run over the full 854-event corpus, not 706. This
means metric values are not expected to match the paper bit-for-bit — see Sections 3-4 for how close
they come regardless, and why the direction of the remaining gap makes sense (854 includes 85 more
`other_gunshot` hard negatives and ~62 more `impulse_noise` samples than whatever produced 706).

## 2. Sample rate: 48 kHz recordings, 22.05 kHz processing - not a real conflict

Actual `dana_artillery` WAV headers are 48 kHz/16-bit/mono, not 22.05 kHz as the paper's acquisition
description ("PreSonus PRM1 ... Roland Rubix 44 ... 22.05 kHz") might suggest. This resolved cleanly:
`ml/utils.py`'s `load_signal` calls `librosa.load(file_name)` with no explicit `sr=`, and librosa
resamples every input to its default target rate, **22050 Hz**, regardless of the file's native rate
(verified directly: loading a 48 kHz source file produces `sr == 22050`). So the paper's "22.05 kHz"
describes the **processing pipeline's** operating rate, not the capture hardware's native rate. No
code change needed; worth a one-sentence clarification in the paper text.

## 3. Reproduction result: archived reference model (Priority 1)

`archive/models/najlepsi_model.h5` loads cleanly under TensorFlow 2.21.0/Keras 3.15.0 and reports
exactly **72,193 trainable parameters**, matching the paper. This, plus `ml/main_cnn.py:66`
(`model.save("najlepsi_model.h5")`) and its 80/20 stratified split matching the paper's stated
methodology, is strong evidence this is the actual model behind the published numbers.

Waveform-domain SNR sweep (`task features:robustness` + `task robustness:pc`) over the full
854-event corpus, using the corrected SNR noise formula (Section 5):

**Desktop float32 (paper Table II):**

| SNR | Paper Acc | Repro Acc | Paper MCC | Repro MCC |
|---|---|---|---|---|
| 30 dB | 84.41% | 84.43% | 0.54 | 0.52 |
| 20 dB | 82.86% | 82.08% | 0.54 | 0.49 |
| 10 dB | 81.12% | 78.34% | 0.49 | 0.43 |
| 5 dB  | 80.44% | 76.46% | 0.46 | 0.36 |

**PC int8 (paper Table III, PC int8 rows):**

| SNR | Paper Acc | Repro Acc | Paper MCC | Repro MCC |
|---|---|---|---|---|
| 30 dB | 98.66% | 97.78% | 0.92 | 0.86 |
| 20 dB | 96.77% | 94.96% | 0.83 | 0.74 |
| 10 dB | 92.74% | 87.35% | 0.69 | 0.55 |
| 5 dB  | 92.41% | 85.71% | 0.65 | 0.42 |

Full per-run CSVs: `generated/reports/archived_model_robustness.csv` (gitignored, regenerate with
`task features:robustness && task robustness:pc -- --model-keras archive/models/najlepsi_model.h5
--model-tflite archive/models/model.tflite --include-clean`).

**Assessment**: same trend, same order of magnitude, consistently a bit lower than the paper,
growing with noise level. This is the expected direction given the corpus is larger (854 vs. 706)
and includes more diverse `other_gunshot` hard negatives the paper's evaluation set may not have had
- more opportunities for false positives, which hurts precision/MCC more as recall is preserved.

**Correction (2026-08-14, supersedes the paragraph originally here) — this is NOT a quantization
effect.** The original text of this section claimed the paper's most-scrutinized claim - "int8
quantization is consistently more robust than float32 on the same waveform-domain noise" -
"reproduces cleanly and independently here" (30 dB MCC 0.52 -> 0.86, 5 dB MCC 0.36 -> 0.42, i.e. the
Table II row above vs. the Table III row above). That comparison is between
`najlepsi_model.h5` (float32) and `archive/models/model.tflite` (int8) - **the exact pair that
`REPRODUCTION_ROADMAP.md` milestone M1 later proved are two different trained checkpoints, not a
float32/int8 pair of the same model** (dequantized-weight correlation ~0 vs. 0.9999+ for a true
requantization control; 15% of clean-sample predictions flip between them). A follow-up controlled
test on the *actual* same model (`generated/models/reconverted.tflite`, a genuine int8
quantization of `najlepsi_model.h5`) found quantization has a negligible-to-slightly-negative
effect on robustness across 5 noise seeds (mean MCC change -0.0115), not a positive one. See
`generated/reports/reviewer_response.md` section 2, which formally retracts the claim this
paragraph originally made. The MCC values above (0.52, 0.86, etc.) are still valid as
provenance/cross-check numbers for the two archived artifacts individually - only the
"quantization improves robustness" interpretation of comparing them is wrong.

## 4. Reproduction result: retrained from scratch (Priority 2)

`ml/generate_manifest.py` was changed to use a stratified 80/20 `sklearn.model_selection.train_test_split`
(`random_state=42`, matching `ml/main_cnn.py:20-24`) instead of the previous 70/15/15 SHA1 hash-bucket
split, which did not match the paper's stated methodology. `ml/train_model.py` now uses the test split
as Keras' `validation_data` (matching `main_cnn.py:45` - there is no separate held-out validation set
in the paper's methodology; see Section 6).

Retraining from scratch on the full pipeline (`task manifest features train convert`) reproduces the
exact architecture (907,072-byte `.h5`, identical to the archived model's file size) and a plausible,
but not identical, model:

- Clean test-split (`task validate:pc`): accuracy 92.4%, precision 0.50, recall 1.00, F1 0.67, MCC 0.68.
- Full-corpus float32 sweep: MCC 0.70 (clean) -> 0.52 (30 dB) -> 0.15 (20 dB) -> **negative at 10/5 dB**
  (recall collapses to 0 - the retrained model stops detecting artillery launches at higher noise).

**This is a real, notable difference from the archived model**, not a bug: the retrained model
matches or exceeds the archived one on clean/low-noise data but degrades far more sharply at 10-5 dB.
Likely causes (not isolated further, out of scope here): no early stopping/model selection beyond a
fixed 50 epochs, and a training corpus that now includes many more `other_gunshot`/`impulse_noise`
hard negatives than whatever the archived model saw, changing what the decision boundary optimizes for.

**Seed sensitivity**: `ml/train_model.py` and `ml/prepare_features.py` had no fixed random seed before
this cleanup (both now default to `--seed 42`, plus `tf.random.set_seed`). Retraining with `--seed 42`
vs. `--seed 7` gave val_accuracy 0.795 vs. 1.000 and train accuracy 0.696 vs. 0.975 respectively, on
the exact same data and hyperparameters - a large swing from weight-initialization alone. The
Section 4 numbers above use the better-converging seed 7 run. This directly corroborates Review 3's
concern about training stability on this small, imbalanced dataset (45 minority-class training
samples before augmentation) and is a good candidate for the review-response phase (e.g. early
stopping on a real validation split, or reporting a distribution over seeds rather than one run).

## 5. Bugs fixed during cleanup

- **`ml/prepare_features.py`**: waveform noise generation used a fixed, non-dB `NOISE_FACTOR = 0.0316`
  heuristic (`signal + noise_factor * noise`), not the SNR-based formula the paper describes. Replaced
  with `add_snr_noise()`, the correct `noise_power = signal_power / 10**(snr_db/10)` formula (ported
  from `ml/plot_signal_with_noise.py:5-12`, the only place it previously existed correctly), and
  extended to sweep `--snr-db 30 20 10 5` (configurable) instead of one fixed level.
- **`ml/evaluate_pc.py` (`predict_tflite`)**: int8 dequantization computed `(output[0][0] - zero_point) *
  scale` where both operands could be narrow numpy int8 types; under numpy 2.x's NEP 50 casting rules
  this silently overflows/wraps (e.g. `127 - (-128)` wrapping instead of `255`), confirmed via a
  reproducible `RuntimeWarning: overflow encountered in scalar subtract`. Fixed by widening both
  operands to Python `int` before subtracting. This directly affects every "PC int8" number reported
  above and in `evaluate_robustness.py` (which reuses this function) - without the fix, PC int8
  robustness numbers would be silently wrong.
- **No script previously reproduced Table II/III at all**: the correct SNR formula only existed in a
  single-file plotting script; the only full-corpus SNR-sweep scripts injected noise into the MFCC
  domain instead of the waveform domain (wrong protocol vs. the paper); the only waveform-domain-noise
  evaluation scripts used one fixed noise level and hardcoded folders that no longer exist. Added
  `ml/evaluate_robustness.py`, wired into `Taskfile.yml` as `features:robustness` + `robustness:pc`.

## 6. Preserved as-is (not bugs, but worth flagging for the review-response phase)

- `class_weight = {0: 1.0, 1: 4.0}` in `train_model.py`/`main_cnn.py` is a hardcoded literal, not
  derived from the actual ~12:1 train-split imbalance. Identical in both the legacy and new pipeline,
  so treated as an intentional (if unexplained) hyperparameter from the original work, not "fixed."
- The test split doubles as Keras `validation_data` during training (no separate held-out validation
  set) - matches the paper's methodology and Review 3's/Review 4's concern #3 about this exact
  practice. Not changed here; changing it would stop this pipeline from reproducing the paper.
- `firmware/esp32s3/sdkconfig` builds with `CONFIG_COMPILER_OPTIMIZATION_DEBUG=y` (Debug, not
  Release/Perf). Left unchanged since this is plausibly what produced the paper's reported ~32 ms
  latency figure; switching to Release would likely change the number, not "fix" it.

## 7. Dependency versions (no original lockfile existed)

The original thesis source (`projects/diplomka_zdrojaky`) was never committed to this repository (no
trace in `git log --all`), so no lockfile/`pip freeze` from the original work exists anywhere.
`requirements.txt` is now pinned to the versions that installed and ran the full pipeline cleanly on
2026-07-10 (TensorFlow 2.21.0/Keras 3.15.0, librosa 0.11.0, numpy 2.4.6, pandas 3.0.3, scikit-learn
1.9.0, scipy 1.17.1, pyarrow 25.0.0, pyserial 3.5, matplotlib 3.11.0). This is a best-effort pin, not
a guarantee of matching whatever the original student environment was; treat as a known limitation.

## 8. Firmware / ESP32-S3 status

Not executed in this environment - no ESP-IDF toolchain and no reachable Docker daemon were available
in this sandbox (`docker info` fails to connect to the daemon socket), so `idf.py build` and any UART
hardware validation were not run here.

What was done instead:
- `ml/validate_esp_uart.py` now computes accuracy/precision/recall/F1/**MCC** (previously missing) and
  average latency, porting the aggregation logic from `firmware/esp32s3/main/audio_add_noise_eval.py`
  (which used hardcoded folders that no longer exist in this repo) and parametrizing it by feature
  variant/SNR level instead. It is ready to run against real hardware but is untested against a board.
- `ml/convert_model.py`'s output (`generated/models/model_data.h`) was checked for structural
  consistency with what firmware currently ships (`firmware/esp32s3/main/model_data.h`): same
  `gunshot_model_int8_tflite` symbol name and array format that `main.cpp:63` expects, comparable size
  (81,400 vs. archived 81,008 bytes). **The firmware's shipped `model_data.h` was deliberately left
  unchanged** (still the archived reference model, bit-identical to `archive/models/model_data.h`) -
  it was never regenerated by this pipeline before, and swapping it for the retrained model (Section 4,
  which is measurably less noise-robust) without hardware validation would be premature.
- ESP-IDF version is inconsistent across three sources: `firmware/esp32s3/main/idf_component.yml`
  declares `idf: '>=4.1.0'`, `firmware/esp32s3/dependencies.lock` was resolved against IDF `5.2.1`,
  and `.devcontainer/fw-devcontainer/Dockerfile` installs `espressif/idf:v5.4`. Not reconciled here
  (documented only) since it needs an actual build to verify compatibility.

**Next step for whoever has a board**: run `task setup` inside `fw-devcontainer`, `task firmware:build`,
flash, then `task validate:esp ESPPORT=<port>` (optionally with `--variant noise_snr10db` etc. from
`ml/validate_esp_uart.py`) to get real Table III ESP32-S3 numbers against the current codebase.

## 9. Out of scope (deferred to the review-response phase)

Not addressed here, on purpose: real ESP32-S3 hardware measurement; dataset licensing/publication
(Review 2); comparison to Elkarous et al. 2025 and other related work; acronym definitions;
power/energy measurement; alternative model families (one-class SVM/isolation forest/autoencoder,
Review 3).

**Resolved since this file was written** (see `REPRODUCTION_ROADMAP.md` M1-M4 and
`generated/reports/reviewer_response.md` for the full evidence trail):
- **PC-int8 vs. ESP32-S3 discrepancy (Review 4 concern #1):** the archived float32 and int8 models
  turn out to be two different trained checkpoints of the same architecture, not a float32/int8
  pair of one model (M1). A candidate bug-based explanation (int8 dequantization overflow in the
  legacy PC script) was tested directly and **refuted** — it would force PC-int8 recall to exactly
  0 at every SNR, which contradicts the published numbers (M4).
- **"Quantization improves robustness" claim (Review 3, Review 4 concern #2):** a controlled
  same-model float32-vs-int8 comparison over 5 noise seeds shows quantization has a negligible-to-
  slightly-negative effect on robustness, not a positive one. The original claim is explained as an
  artifact of comparing two different trained models (M1).
- **Held-out generalization test (Review 3, Review 4 concern #3):** `ml/evaluate_robustness.py` now
  supports `--split test`; multi-seed held-out results (mean ± std over seeds 42-46) exist alongside
  full-corpus results (M2, M3). Caveat carried forward: the held-out split has only 13 launch events.

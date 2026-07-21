# BEC2026 Reviews — Paper 53 (ACCEPTED) + Camera-Ready Notes

Received 2026-07-21. Paper **accepted** to BEC2026. Reviews below verbatim; must be addressed in final (camera-ready) version.

## Key dates

- **2026-08-31 — final (camera-ready) version due**
- 2026-08-31 — early bird registration deadline
- 2026-09-18 — regular registration deadline
- 2026-10-05 — "DeepTech Commercialization" for PhD students, TalTech Student House (STU)
- 2026-10-06 to 10-08 — technical conference, Tallinn, Estonia
- IEEE Xplore compatibility validation + IEEE Copyright instructions to be sent separately

## Scores

| Review | Score |
|--------|-------|
| R1 | 2 (accept) |
| R2 | 1 (weak accept) |
| R3 | 2 (accept) |
| R4 | 1 (weak accept) — recommends major revision |

## Cross-review themes (action items for camera-ready)

1. **PC int8 vs. ESP32-S3 int8 discrepancy** (R2 #6, R3, R4 major concern 1 + Q1): same quantized model, same inputs should give near-identical results on both platforms, but Table III shows consistent differences (e.g., 30 dB MCC 0.92 vs. 0.99; 5 dB precision/recall inverts 0.53/0.90 vs. 0.95/0.70). Must explain (inputs, scaling, operator implementations) — headline results depend on it.
2. **"Quantization improves robustness" claim not established** (R3, R4 major concern 2 + Q2): float32 → int8 improvement is counterintuitive; rule out preprocessing/scaling differences before presenting as robustness benefit, or reframe as open observation.
3. **Evaluation partly on training data** (R3, R4 major concern 3 + Q3): stress test runs on all 706 events, most seen in training; no train/val/test split described. Report held-out results; headline 0.99→0.80 MCC figures need this caveat or replacement.
4. **Two-stage framing vs. single-stage evaluation** (R1 title/abstract/III.A, R4 #4): title says two-stage pipeline but only post-trigger classifier evaluated. Revise title, state in abstract that embedded experiment validates only post-trigger classifier, summarize/reference trigger-stage validation, remind reader in discussion/conclusion that results assume successful trigger localization.
5. **Dataset imbalance and small size** (R1 III.B, R3): 62 launch vs. 644 non-launch events. Discuss generalization impact; R3 suggests data augmentation; report artillery types/calibers, environmental contexts, standoff ranges (R4 #7, Q5).
6. **MFCC on host, not on-device** (R1 III.F/VI, R4 #6, Q7): clarify reported latency is classifier inference only, not end-to-end; clarify intended host/MCU partitioning; on-device MFCC + end-to-end latency = future work.
7. **Related work: trim + strengthen positioning** (R1 Sec II, R2 #1): shorten Section II, add key quantitative results, reallocate space to dataset/training/deployment details. Emphasize novelty vs. state of the art; R2 points to recent parallel work: Elkarous, L., Jeridi, M.H., Dhouibi, M., 2025. "Firearm classification from acoustic signals using combined mel spectrogram, MFCC, LFCC, and CRNN networks." Scientific Reports, 15(1), p.44200. Consider fewer references (29 high for 6-page paper).

## Smaller fixes

- Report final model size in bytes, Flash usage, RAM usage; state whether all ops natively supported by TFLite Micro on ESP32-S3 (R1 III.C).
- Elaborate on "float32 reference fails mainly through false-positive inflation rather than missed artillery launches" (R1 Sec V).
- Move some limitations earlier in paper (continuous-stream false-trigger rate, MFCC not on-device, end-to-end latency) (R1 Sec VI).
- Grammar/wording pass (R1).
- Define acronyms on first use in abstract and body (R2 #2).
- Add references for used techniques/methods incl. int8/float32 quantization (R2 #3).
- Figures 1 & 2: normalized amplitude is unitless — drop empty unit symbol (R2 #4); figures near-duplicative, consider consolidating (R4 minor).
- Consider publishing dataset as additional contribution, with acquisition setup specs, labels, meta info (source, distance, direction) (R2 #5, R4 #7).
- Table I: horizontal separator before "Main clean corpus" (R1 III.B).
- Define "acoustic SNR": per-event signal/noise power reference and scaling (R4 minor, Q4).
- Justify 22.05 kHz sampling choice (R4 #8, Q6 — why not 44.1 kHz).
- State why ESP32-S3 was chosen (cost, energy, size, availability) (R4 #5, Q7).
- No power/energy measurement despite low-power motivation — acknowledge or add (R4 minor).
- R3: consider discussing anomaly-detection literature (one-class SVM, Isolation Forest, autoencoders) as alternative low-footprint baselines, and justify CNN choice.

---

## Repo findings (code audit 2026-07-21)

Three of the reviewers' hardest technical complaints already have candidate explanations in the codebase:

1. **PC-int8 vs ESP32-int8 discrepancy — prime suspects:**
   - `ml/eval_tflite_pc.py:66-68` (legacy PC-int8 script): **int8 overflow in output dequantization** (`output - zero_point` without type widening; numpy 2.x wraps, e.g. `127-(-128)`). Fixed version exists in `ml/evaluate_pc.py:49-53`. ESP32 C code (`firmware/esp32s3/main/main.cpp:160-164`) promotes correctly — no overflow. If Table III PC-int8 numbers came from the legacy script, the discrepancy is a bug artifact.
   - **Different weights:** firmware ships the archived model (`model_data.h`, 81008 B, bit-identical to `archive/models/model_data.h`); pipeline `ml/convert_model.py` emits a different binary (~81400 B). PC eval and ESP32 may have compared different models.
   - Minor: decision threshold `>` (`ml/validate_esp_uart.py:69`) vs `>=` (`ml/evaluate_pc.py:68`, `ml/evaluate_robustness.py:55`); numpy banker's rounding vs C `round()` at .5 boundaries.
2. **Eval on training data — confirmed:** `ml/evaluate_robustness.py` runs on the full corpus (all splits). But a split exists (80/20 stratified, seed 42, `datasets/recordings/splits.csv`) → held-out eval is just filtering. Caveat: test split has only **13 launch events** (158 non-launch) → noisy statistics, wide intervals.
3. **Event-count mismatch (reviewers did not catch):** paper claims 706 events (62/644); manifest has **854 (63 launch / 791 non-launch)**; 706 equals only the `impulse_noise` label count. Provenance must be resolved before any re-run (`REPRODUCTION_NOTES.md:10-24`).
4. Other facts: model 72,193 params; full-integer int8 (repr. dataset = first 100 train MFCCs, `ml/convert_model.py`); tensor arena 80 KiB; latency measured only around `Invoke()` (no MFCC/UART) and with **Debug build** (`sdkconfig`: `CONFIG_COMPILER_OPTIMIZATION_DEBUG=y`) — affects the ~32 ms figure; no power measurement code exists; manifest metadata `distance`/`caliber`/`angle`/`suppressor` empty for all rows (dataset details must come from measurement-campaign notes, not the repo).

## Decisions (defaults — confirm with co-authors/supervisor)

- **Do not retrain.** Canonical model = archived int8 from firmware; only unify evaluation. Retraining changes all published numbers → rewrite of entire Results.
- **Dataset likely not published** (military measurements) — state "available on request" or reason for restriction (answers R2 #5, R4 Q5).
- **Power measurement: skip** — acknowledge as future work (R4 minor).
- **Title:** narrow to single-stage, e.g. "A Lightweight Post-Trigger Acoustic Classifier for Embedded Artillery Launch Detection".

## Work plan (1 day/week, 4–8 h; deadline 2026-08-31)

| Day | Week of | Work | Hours |
|-----|---------|------|-------|
| 1 | Jul 28 | Forensic audit of published numbers (706 vs 854; which script produced Table III) + unified eval harness: archived .tflite, manifest features, `>=` threshold, fixed dequantization, per-sample CSV output. Re-run PC float32 + int8, full corpus + held-out. | 4–6 |
| 2 | Aug 4 | ESP32 re-run (hardware needed): patch `validate_esp_uart.py` (threshold, log raw scores), identical inputs over UART, per-sample PC-int8 vs ESP32-int8 comparison (expect ≤1 LSB). Re-measure latency with Release build; Flash/RAM from `idf.py size`; verify TFLM native op support (R1 III.C). | 4–8 |
| 3 | Aug 11 | New Table III + held-out results; SNR definition into Experimental Setup (formula from `ml/prepare_features.py:31-36`); decide quantization narrative (explained artifact vs open observation); explain float32 FP inflation on new numbers; add model size/Flash/RAM to text. | 4–6 |
| 4 | Aug 18 | Writing pass 1: title, abstract (post-trigger-only + held-out caveats), Sec II trim + Elkarous 2025 + contribution gap, Sec III (trigger-stage reference, dataset details, why CNN / ESP32-S3 / 22.05 kHz, host/MCU MFCC partitioning), move limitations earlier. | 4–8 |
| 5 | Aug 25 | Writing pass 2: acronyms, references (add int8/quantization refs, reduce total count), Figs 1&2 units/consolidation, Table I separator, grammar pass, send to co-authors. | 4–6 |
| 6 | Aug 31 | Buffer: co-author comments, IEEE PDF eXpress validation, copyright form, submit. **Same day = early-bird registration deadline.** | 2–4 |

## Verification criteria

- PC-int8 through archived .tflite should match published ESP32 numbers per-sample (if bug hypothesis holds) — check MCC/precision/recall against Table III.
- After day 2: per-sample |PC-int8 − ESP32-int8| score ≤ 1 LSB → discrepancy resolved; otherwise bisect (input quantization, operator versions).
- Final PDF: 4–6 pages, passes IEEE PDF eXpress.

## Risks

- New numbers differ substantially from published → more rewriting (buffer in days 3 and 6).
- Discrepancy survives unification → honest reframe as open observation (acceptable for camera-ready).
- Hardware unavailable in week 2 → swap days 2 and 3.

---

## Verbatim reviews

SUBMISSION: 53
TITLE: A Lightweight and Robust Two-Stage Acoustic Pipeline for Embedded Artillery Launch Detection

----------------------- REVIEW 1 ---------------------

SUBMISSION: 53
TITLE: A Lightweight and Robust Two-Stage Acoustic Pipeline for Embedded Artillery Launch Detection

----------- Overall evaluation -----------
SCORE: 2 (accept)
----- TEXT:
The paper is timely and relevant for BEC2026. The proposed approach is interesting and its potential is illustrated by experimental results.

The following aspects should be taken into account for improving the paper:

Title: the title refers to a two-stage acoustic pipeline, but the paper seems to evaluate only the post-trigger classification stage. Consider revising the title accordingly.

Abstract: the abstract includes numerical results, which is appreciated. However, consider explicitly stating that the embedded experiment validates only the post-trigger classifier.

Section II: the related work section is quite thorough, but it is relatively long compared with the experimental part of the paper. Consider the following: 1a) shorten the wording in the section and 1b) add a few key quantitative results, and 2) use the saved space to provide more details on the dataset, training, and deployment protocol.

Section III.A: since the title emphasizes a two-stage pipeline, briefly summarize the trigger performance or provide a clear reference to how/where it has been validated.

Section III.B: the dataset is small and seems to be strongly imbalanced (62 artillery launch events vs. 644 non-launch events). Elaborate on how this might affect the generalization of the results. Moreover, in Table I, add an horizontal separator before “Main clean corpus”.

Section III.C: it would be valuable to report the final model size in bytes, Flash usage, RAM usage, and whether all operations are supported natively by TensorFlow Lite Micro on ESP32-S3.
Section III.F: Do I understand correctly that the embedded validation uses host-computed MFCC tensors transmitted to the ESP32-S3? If it is so, make it clearer that the reported latency is only for the classifier inference, and not end-to-end latency.
Section V: elaborate on “The float32 reference therefore fails mainly through false-positive inflation rather than missed artillery launches”.

Section VI: the discussion of limitations is good and valuable. I suggest moving some of these earlier in the paper (e.g. issues of continuous-stream false-trigger rates, MFCC not deployed on-device, end-to-end latency).

Other comments:
Check grammar and wording throughout the paper.
Consider reducing the number of references (29 references is one the higher end for a 6-page conference paper).


----------------------- REVIEW 2 ---------------------

SUBMISSION: 53
TITLE: A Lightweight and Robust Two-Stage Acoustic Pipeline for Embedded Artillery Launch Detection

----------- Overall evaluation -----------
SCORE: 1 (weak accept)
----- TEXT:
The paper is timely and relevant for BEC2026
This work aims to validate the practical performance of an embedded platform-deployed lightweight MFCC-CNN second-stage classifier for acoustic artillery launch detection under controlled waveform-domain perturbations. This work covers the practical acquisition of acoustic artillery launch & impact data, mixed with various non-artillery (false positive) samples for the classification model training. The performance of the targeted embedded system-deployed int8 quantized classifier was then evaluated and compared to the desktop deployment of the same quantized int8 inference, as well as the desktop-deployed float32 inference. Results have confirmed the reliable performance of the prepared classifier for acoustic artillery launch detection on the embedded ESP32 platform.


Title:
The title generally reflects the main focus of this work. However, it could be improved to better reflect the main contribution.


Comments & questions:

1.      The contribution and the novelty of this work need to be clearly emphasized, along with the addressed state-of-the-art gap, as the acoustic firearm/artillery detection and classification itself is not a novel topic.

For instance, a series of works in this field was presented in 2006-2008 by the Grasing, Morcos, and Desai team (some of their work is also cited in the given paper). One of the recent works on firearm detection and classification (utilizing similar methods and addressing the embedded deployment) was published in Dec. 2025:

- Elkarous, L., Jeridi, M.H., and Dhouibi, M., 2025. Firearm classification from acoustic signals using combined mel spectrogram, MFCC, LFCC, and CRNN networks. Scientific Reports, 15(1), p.44200.

*The referred sample work is rather recent (parallel), and is provided to encourage the authors to further emphasize and clarify the contribution of the proposed work with respect to the state of the art.

2.      Acronyms need to be defined upon their first use in both the abstract and the main body

3.      It would be beneficial to include references for the used techniques, methods (and quantization levels, e.g., int8, float32) to improve the accessibility of the work.

4.      In Figures 1 & 2, the normalized amplitude is a unitless metric that does not require an empty unit symbol.

5.      This work also covers the acquisition of actual artillery acoustic data samples, which may be relevant for the given field of research. It would be beneficial for this work to include the used dataset as an additional contribution (unless the data sharing is limited/restricted). All of the relevant information may be combined (e.g., specifications of the data acquisition setup, labelled acoustic data with meta information, such as true source of acoustic event, distance, direction (if the directional data, or synchronized readings from each separate microphone are available), etc.)

6.      According to the provided description, the same int8 quantized inference is performed on the targeted embedded platform and a desktop setup as a reference. However, the results in Table III demonstrate a noticeable and consistent performance difference between desktop and embedded deployment of the same int8-quantized classifier at each SNR level. For instance, the desktop deployment shows approximately 10% worse average performance in Accuracy, F1, MCC; up to 40% worse performance in the case of Precision; along with up to 20% improved Recall. What could be the reason behind this? These outcomes need to be addressed and explained/discussed either in Section V.C "ESP32-S3 Deployment Results" or in Section VI. "DISCUSSION AND LIMITATIONS".


----------------------- REVIEW 3 ---------------------

SUBMISSION: 53
TITLE: A Lightweight and Robust Two-Stage Acoustic Pipeline for Embedded Artillery Launch Detection

----------- Overall evaluation -----------
SCORE: 2 (accept)
----- TEXT:
This paper presents research aimed at detecting artillery fire using a passive, low-cost system.
Given that the objective is to classify whether an acoustic sample represents a shot or not, it would have been interesting to examine in greater detail the literature on anomaly detection, whether in acoustic or non-acoustic applications.
You have constructed the dataset yourselves, which is understandable given the application context, but we do not have information on the number of different types of artillery pieces used to generate this dataset, nor the number of different environmental contexts.
As your dataset is unbalanced, why not try data augmentation, for example, to 1) increase the amount of data and 2) better balance your dataset? Training on such a small amount of data can lead to biased results.
Regarding your model, you are using a CNN without us knowing why you made that choice. If I’ve understood correctly, it seems you want to distinguish an artillery shot from among the recorded sounds, which is similar to anomaly detection. Other models with a low computational and memory footprint could have been tested, such as one-class SVMs, Isolation Forest or even autoencoders.These models perform well in this type of application and can be easily implemented on a microcontroller.
As regards your results, you are testing your model’s performance on your dataset without first dividing it into a training set, a validation set and a test set. This standard division of the dataset is carried out at random in order to validate the results. We then usually use another dataset to see whether the model has learnt to generalise.
In this case, your results are not necessarily convincing, as it appears that you are testing your model on the data used to train it.
In the results section, you present the performance of your model on a PC using float32, then on a PC using int8 (i.e. after quantisation), and the same quantised model on an ESP32.
You do not explain why this is the case, nor do you clarify whether it is normal for your model, when quantised on a PC, to perform better than the same model in float32 (which is counterintuitive). The same applies to the results on the ESP32: why does the same model, when run on a different target but using the same dataset, yield different results once again?
This would warrant further analysis and possible explanations.
The work presented in the article is, however, interesting, but it should be supplemented and validated using other data that were not encountered during the training.


----------------------- REVIEW 4 ---------------------

SUBMISSION: 53
TITLE: A Lightweight and Robust Two-Stage Acoustic Pipeline for Embedded Artillery Launch Detection

----------- Overall evaluation -----------
SCORE: 1 (weak accept)
----- TEXT:
SUMMARY
-------
The paper evaluates the post-trigger classifier of a two-stage acoustic
artillery-launch detection system. A compact CNN classifies 60 ms peak-centered
windows as launch vs. non-launch over a 706-event corpus (62 launches / 644
non-launches). It is tested under added Gaussian noise at SNR = 30/20/10/5 dB in
three regimes (desktop float32, PC int8, ESP32-S3 int8 via TFLite Micro).
Headline: MCC 0.99 -> 0.80 across 30 -> 5 dB on ESP32-S3, ~32 ms/segment,
80 KiB tensor arena.

STRENGTHS
---------
1. Clear scoping to the second stage and a candid limitations section.
2. Reproducible MFCC front end (framing, hop, window, FFT size, mel/coefficient
   counts fully specified).
3. Concrete deployment evidence: parameter count, arena size, selective operator
   set, and measured latency.
4. Thorough, well-organized related work.

MAJOR CONCERNS
--------------
1. The three inference regimes disagree with each other. The same quantized
   model gives materially different results on PC int8 vs. ESP32-S3 (e.g., 30 dB
   MCC 0.92 vs. 0.99; at 5 dB the precision/recall balance inverts: 0.53/0.90
   vs. 0.95/0.70). Running one fixed integer model on the same inputs should
   give essentially identical numbers on both platforms. This discrepancy needs
   to be explained (identical inputs, scaling, and operator implementations?),
   because the headline results depend on it.

2. The claimed "quantization improves robustness" effect is not established.
   Going from float32 to int8 raises performance substantially, which is
   unusual; and if quantization were the cause, the two int8 regimes (concern 1)
   should agree, but they do not. A simpler explanation - differences in
   preprocessing or scaling between the float and int8 pipelines - has not been
   ruled out. This should be investigated before the effect is presented as a
   robustness benefit rather than reframed as an open observation.

3. The headline robustness numbers are derived from corpus-level stress tests over the full dataset, rather than exclusively from held-out data.
   The noise stress test runs on all 706 events, most of which were used in
   training. The abstract and conclusion present the 0.99/0.80 figures without
   this caveat; results on data not seen during training should be reported and
   used for the headline claim.

SYSTEM-DESIGN AND DATASET CLARITY
---------------------------------
4. End-to-end dependence on the trigger. Because the 60 ms segments are centered
   on trigger-detected peaks, all results assume localization already succeeded;
   end-to-end performance therefore depends on the (out-of-scope, prior-work)
   triggering stage. Briefly remind the reader in the discussion/conclusion that classifier performance assumes successful trigger localization from the previously validated first stage.

5. Platform motivation. State explicitly why ESP32-S3 was chosen (cost, energy,
   size, or availability).

6. Front-end placement / purpose of the MCU. In the validation protocol, MFCC
   extraction runs on the host. Clarify the intended deployment partitioning between host and MCU, and whether MFCC extraction is expected to run on-device in the target system. On-device MFCC and end-to-end latency should be quantified in future work.

7. Dataset details. State whether the corpus is public; report the launch
   standoff range/distances and expected operating range; and address behavior
   across different artillery calibers.

8. Sampling configuration. Briefly justify the choice of 22.05 kHz sampling for the intended deployment scenario.

MINOR ISSUES
------------
- "Acoustic SNR" is undefined - specify the per-event signal/noise power
  reference and scaling used to set each SNR level.
- No power/energy measurement despite the low-power motivation; only latency and
  memory are reported.
- Figures 1 and 2 are near-duplicative; consider consolidating.

QUESTIONS FOR THE AUTHORS
-------------------------
1. What accounts for the differing PC int8 vs. ESP32-S3 results for the same
   quantized model?
2. Can you rule out preprocessing/scaling differences (rather than quantization)
   as the source of the float32 -> int8 improvement?
3. What are the results on data not used in training?
4. How is per-event SNR defined and applied?
5. Is the dataset public, and what standoff ranges and calibers does it cover?
6. Is "22.05 kHz" the sample rate; if so, why not 44.1 kHz?
7. Why ESP32-S3 specifically, and where does MFCC extraction run in the intended
   deployment?

OVERALL ASSESSMENT
------------------
The deployment contribution is real and clearly presented, and the authors'
transparency about scope is a strength. However, the robustness claim rests on
results that disagree across the three inference regimes, an effect
(quantization) whose cause is not established, and headline numbers measured
partly on training data. Several system-design and dataset questions (range,
calibers, sampling rationale, platform motivation, front-end placement, power)
also remain open.

Recommendation: Major revision. The deployment feasibility result is
publishable, but the authors should (a) resolve the PC-int8/ESP32 discrepancy,
(b) report results on data not used in training, (c) explain or reframe the
quantization effect, and (d) address the dataset/system-design clarifications.

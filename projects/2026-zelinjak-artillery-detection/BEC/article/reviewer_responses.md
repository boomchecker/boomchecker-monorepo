# Odpovědi recenzentům — BEC2026 Paper 53 (camera-ready)

Znění připomínek je vykopírováno **verbatim** z recenzí (zdroj:
`BEC/.agents/memories/2026-07-21_bec2026_reviews_camera_ready.md`). U nestrukturovaných
recenzí (R3) je text pouze rozdělen na odrážky po větách — beze změny znění. Odpovědi
se píší do bloků **Odpověď:**.

Podkladové dokumenty: `generated/reports/reviewer_response.md` (M1–M6 důkazy),
`generated/reports/weights_provenance.md`, `generated/reports/provenance_table3.md`,
`BEC/retraining/README.md` (retrénink, ablace augmentace, duplicity).

---

## REVIEW 1 — SCORE: 2 (accept)

> The paper is timely and relevant for BEC2026. The proposed approach is interesting and its potential is illustrated by experimental results.

### R1.1 Title

> Title: the title refers to a two-stage acoustic pipeline, but the paper seems to evaluate only the post-trigger classification stage. Consider revising the title accordingly.

**Odpověď:**

We agree. The title has been changed to *"A Lightweight Noise-Robust Post-Trigger Classifier for Embedded Acoustic Artillery Launch Detection"*, which names exactly what the paper evaluates: the post-trigger (second-stage) classifier. The two-stage architecture is still described in Section III.A as the system context, with the trigger stage explicitly referenced as previously validated prior work.

### R1.2 Abstract

> Abstract: the abstract includes numerical results, which is appreciated. However, consider explicitly stating that the embedded experiment validates only the post-trigger classifier.

**Odpověď:**

Added. The abstract now states explicitly: "The embedded experiment validates only the post-trigger classifier: deployed on ESP32-S3 with TensorFlow Lite Micro, it preserves high performance under the tested conditions, ..."

### R1.3 Section II (related work)

> Section II: the related work section is quite thorough, but it is relatively long compared with the experimental part of the paper. Consider the following: 1a) shorten the wording in the section and 1b) add a few key quantitative results, and 2) use the saved space to provide more details on the dataset, training, and deployment protocol.

**Odpověď:**

Section II was revised at approximately constant length: three tangential sentences (and two references) were removed, and the freed space now carries (1b) key quantitative results of the cited prior work — 97.5 % launch/impact discrimination and 93.6 % variant classification for the classical mortar/artillery line (Desai et al., Hohil et al.), firearm-identification accuracies above 90 % for CNN approaches (Raponi et al.), and 96.3 % accuracy at 187 ms per 2 s segment on a Raspberry Pi 4 for the recent CRNN classifier (Elkarous et al., 2025) — together with a sharper closing statement of the two questions the paper answers. Regarding (2), the experimental part has grown substantially in the final version: the dataset section now describes the extended multi-microphone corpus with an event-level train/validation/test split, and the results section adds a controlled augmentation ablation, held-out multi-seed evaluation, and extended SNR levels. The total reference count decreased.

### R1.4 Section III.A (trigger stage)

> Section III.A: since the title emphasizes a two-stage pipeline, briefly summarize the trigger performance or provide a clear reference to how/where it has been validated.

**Odpověď:**

The revised title no longer emphasizes a two-stage pipeline (see R1.1), so the trigger stage now appears only as system context. We followed both options suggested by the reviewer: Section III.A now briefly summarizes the trigger's validated performance (100 % detection of tested small-arms gunshots in shooting-range measurements, reliable operation down to an acoustic SNR of 5 dB) with a direct citation to the prior work where it was validated. The discussion additionally notes that the classifier's evaluated SNR range deliberately matches this validated operating envelope of the trigger stage. In addition, the discussion (Section VI) now reminds the reader that each classified segment is centered on a trigger-detected peak, so the reported figures characterize classifier performance under the assumption of successful impulse localization by the previously validated first stage (this also addresses Reviewer 4, point 4). We phrase this as a scope statement rather than a limitation, since the trigger stage is intentionally out of scope.

### R1.5 Section III.B (dataset)

> Section III.B: the dataset is small and seems to be strongly imbalanced (62 artillery launch events vs. 644 non-launch events). Elaborate on how this might affect the generalization of the results. Moreover, in Table I, add an horizontal separator before “Main clean corpus”.

**Odpověď:**

The artillery corpus was enlarged and restructured for the final version: launch events were re-extracted from the original measurement-campaign recordings using all four synchronized microphone channels instead of a single channel, giving 50 physical launch events represented by 146 channel samples (training uses all four channels per event; validation and test use one channel per event so their samples remain independent physical events, with the split performed at the event level to prevent leakage between channels of the same shot). Table I was redesigned accordingly — it now distinguishes independent physical events from per-channel samples, shows the event-level train/validation/test split per subset, lists small-arms gunshots as an explicit hard-negative subset, and includes the requested horizontal separator before the total row. The effect of imbalance and the small positive count on generalization is now addressed explicitly: Section III.B describes the class-weighted training and the event-level split, the results are reported as mean ± std over five training seeds and five noise-realization seeds with the imbalance-robust MCC as the headline metric (Section V), and the limitations section notes that the small number of held-out launch events remains the dominant source of statistical uncertainty.

### R1.6 Section III.C (model size, Flash, RAM, ops)

> Section III.C: it would be valuable to report the final model size in bytes, Flash usage, RAM usage, and whether all operations are supported natively by TensorFlow Lite Micro on ESP32-S3.

**Odpověď:**

_(doplnit)_

### R1.7 Section III.F (host-computed MFCC, latency)

> Section III.F: Do I understand correctly that the embedded validation uses host-computed MFCC tensors transmitted to the ESP32-S3? If it is so, make it clearer that the reported latency is only for the classifier inference, and not end-to-end latency.

**Odpověď:**

_(doplnit)_

### R1.8 Section V (float32 FP inflation)

> Section V: elaborate on “The float32 reference therefore fails mainly through false-positive inflation rather than missed artillery launches”.

**Odpověď:**

_(doplnit)_

### R1.9 Section VI (limitations earlier)

> Section VI: the discussion of limitations is good and valuable. I suggest moving some of these earlier in the paper (e.g. issues of continuous-stream false-trigger rates, MFCC not deployed on-device, end-to-end latency).

**Odpověď:**

_(doplnit)_

### R1.10 Other comments

> Check grammar and wording throughout the paper.
> Consider reducing the number of references (29 references is one the higher end for a 6-page conference paper).

**Odpověď:**

_(doplnit)_

---

## REVIEW 2 — SCORE: 1 (weak accept)

> The paper is timely and relevant for BEC2026
> This work aims to validate the practical performance of an embedded platform-deployed lightweight MFCC-CNN second-stage classifier for acoustic artillery launch detection under controlled waveform-domain perturbations. This work covers the practical acquisition of acoustic artillery launch & impact data, mixed with various non-artillery (false positive) samples for the classification model training. The performance of the targeted embedded system-deployed int8 quantized classifier was then evaluated and compared to the desktop deployment of the same quantized int8 inference, as well as the desktop-deployed float32 inference. Results have confirmed the reliable performance of the prepared classifier for acoustic artillery launch detection on the embedded ESP32 platform.

### R2.0 Title

> Title:
> The title generally reflects the main focus of this work. However, it could be improved to better reflect the main contribution.

**Odpověď:**

The title has been revised to *"A Lightweight Noise-Robust Post-Trigger Classifier for Embedded Acoustic Artillery Launch Detection"*. It now states the main contribution directly: a lightweight (72,193 parameters, 81 kB int8) post-trigger classifier with experimentally demonstrated noise robustness (held-out MCC 0.98 at 5 dB SNR, graceful degradation measured down to −5 dB) deployed and validated on an embedded target.

### R2.1 Contribution and novelty

> 1. The contribution and the novelty of this work need to be clearly emphasized, along with the addressed state-of-the-art gap, as the acoustic firearm/artillery detection and classification itself is not a novel topic.
>
> For instance, a series of works in this field was presented in 2006-2008 by the Grasing, Morcos, and Desai team (some of their work is also cited in the given paper). One of the recent works on firearm detection and classification (utilizing similar methods and addressing the embedded deployment) was published in Dec. 2025:
>
> - Elkarous, L., Jeridi, M.H., and Dhouibi, M., 2025. Firearm classification from acoustic signals using combined mel spectrogram, MFCC, LFCC, and CRNN networks. Scientific Reports, 15(1), p.44200.
>
> *The referred sample work is rather recent (parallel), and is provided to encourage the authors to further emphasize and clarify the contribution of the proposed work with respect to the state of the art.

**Odpověď:**

We agree that acoustic weapon detection itself is not novel, and the revised Section II no longer implies it; it now closes with the two specific questions the paper answers (whether robustness training requires waveform-level rather than cepstral-level perturbation, and whether the result survives int8 MCU deployment). The suggested reference (Elkarous et al., 2025) is now cited with its key figures (96.3 % accuracy, 187 ms per 2 s segment on a Raspberry Pi 4); it addresses firearm-type identification with rich fused features on application-class hardware, whereas our work targets binary launch-vs-confuser discrimination with a minimal-footprint model on MCU-class hardware. The state-of-the-art gap is now stated as a concrete research question at the end of Section II: where robustness-oriented augmentation must be placed relative to the nonlinear cepstral front end of a constrained post-trigger classifier, and whether the resulting behavior survives int8 deployment. The revised results answer this question through a controlled augmentation-domain ablation (same architecture, same protocol): MFCC-domain jitter alone collapses at low SNR, whereas waveform-domain augmentation preserves performance, consistent with — and extending — comparisons reported for noise-robust ASR (Braun et al., 2017).

### R2.2 Acronyms

> 2. Acronyms need to be defined upon their first use in both the abstract and the main body

**Odpověď:**

We audited every acronym in the manuscript and fixed the following: CNN and MFCC are now defined at their first use in the main body (Section I), and definitions were added for ASR, MCU, STFT, and DCT at their first occurrences.

### R2.3 References for techniques

> 3. It would be beneficial to include references for the used techniques, methods (and quantization levels, e.g., int8, float32) to improve the accessibility of the work.

**Odpověď:**

_(doplnit)_

### R2.4 Figures 1 & 2 units

> 4. In Figures 1 & 2, the normalized amplitude is a unitless metric that does not require an empty unit symbol.

**Odpověď:**

_(doplnit)_

### R2.5 Dataset as contribution

> 5. This work also covers the acquisition of actual artillery acoustic data samples, which may be relevant for the given field of research. It would be beneficial for this work to include the used dataset as an additional contribution (unless the data sharing is limited/restricted). All of the relevant information may be combined (e.g., specifications of the data acquisition setup, labelled acoustic data with meta information, such as true source of acoustic event, distance, direction (if the directional data, or synchronized readings from each separate microphone are available), etc.)

**Odpověď:**

We appreciate the suggestion, but data sharing is restricted in this case: owing to the defence-related nature of the measurement campaign, the corpus is not published. The final version nevertheless expands the in-paper documentation of the data along the lines the reviewer suggests: the acquisition setup is specified (four PreSonus PRM1 microphones, Roland Rubix 44 interface, 16-bit audio), the synchronized four-channel recordings of each launch event are now used explicitly (Table I), the measurement geometry is reported (station approximately 1–2 km from the impact area, guns at an estimated 5–10 km standoff), and the corpus composition, labeling, and event-level split are documented in Section III.B.

### R2.6 PC vs ESP32 int8 discrepancy

> 6. According to the provided description, the same int8 quantized inference is performed on the targeted embedded platform and a desktop setup as a reference. However, the results in Table III demonstrate a noticeable and consistent performance difference between desktop and embedded deployment of the same int8-quantized classifier at each SNR level. For instance, the desktop deployment shows approximately 10% worse average performance in Accuracy, F1, MCC; up to 40% worse performance in the case of Precision; along with up to 20% improved Recall. What could be the reason behind this? These outcomes need to be addressed and explained/discussed either in Section V.C "ESP32-S3 Deployment Results" or in Section VI. "DISCUSSION AND LIMITATIONS".

**Odpověď:**

_(doplnit)_

---

## REVIEW 3 — SCORE: 2 (accept)

*(Souvislý text rozdělen po větách na samostatné body — znění beze změny.)*

> This paper presents research aimed at detecting artillery fire using a passive, low-cost system.

### R3.1 Anomaly-detection literature

> Given that the objective is to classify whether an acoustic sample represents a shot or not, it would have been interesting to examine in greater detail the literature on anomaly detection, whether in acoustic or non-acoustic applications.

**Odpověď:**

Section II now discusses this family of methods (one-class SVMs, isolation forests, autoencoders, with a citation to their evaluation on embedded platforms) and explains why we nevertheless use a supervised binary classifier: the classifier operates after an impulsive-event trigger, so it receives a conditional distribution of impulsive candidates in which non-artillery gunfire is a hard negative rather than an outlier — outlierness with respect to the acoustic background is not equivalent to the artillery-launch label. One-class approaches remain relevant alternatives for first-stage novelty detection. Empirically, the supervised formulation is supported by the hard-negative results: the deployed model produces zero false positives on the small-arms gunshot class across all noise levels.

### R3.2 Dataset details (artillery types, environments)

> You have constructed the dataset yourselves, which is understandable given the application context, but we do not have information on the number of different types of artillery pieces used to generate this dataset, nor the number of different environmental contexts.

**Odpověď:**

Section III.B now states this explicitly: the launch recordings originate from two separate measurement days with a single artillery type (a 152 mm self-propelled howitzer, named in Table I), with the recording station approximately 1–2 km from the impact area and the guns firing from an estimated 5–10 km standoff. The limitations section acknowledges that the corpus covers a single artillery type, so generalization across calibers is untested and remains future work.

### R3.3 Data augmentation and dataset size

> As your dataset is unbalanced, why not try data augmentation, for example, to 1) increase the amount of data and 2) better balance your dataset? Training on such a small amount of data can lead to biased results.

**Odpověď:**

We agree, and the final version acts on this suggestion in three ways. (1) The launch class was re-extracted from the original measurement campaign using all four synchronized microphone channels instead of a single channel: 50 physical launch events, each represented by four synchronously cropped channel recordings. Training uses all four channels of each training event (32 events, 128 launch training samples), which provides natural sensor-position diversity; validation and test use one channel per event so that their samples remain statistically independent, and the split is performed at the level of physical events to prevent leakage between channels of the same shot. (2) Data augmentation is applied — and its design turned out to be the central experimental question of the revised paper: every training sample is augmented with additive-noise variants at four SNR levels (five-fold enlargement, 3,170 training samples in total), and a controlled ablation shows that the domain in which this augmentation is applied is decisive (waveform-domain augmentation versus cepstral-domain jitter: held-out MCC 0.98 versus 0.30 at 5 dB SNR for the same architecture). (3) Class imbalance is further mitigated by class-weighted training, and we report MCC as the headline metric because it is robust to class imbalance. The concern about small-data bias is addressed by reporting all results as mean ± standard deviation over five training seeds and five noise seeds, and the limitations section notes the small number of positive events explicitly.

### R3.4 Why CNN; anomaly-detection alternatives

> Regarding your model, you are using a CNN without us knowing why you made that choice. If I’ve understood correctly, it seems you want to distinguish an artillery shot from among the recorded sounds, which is similar to anomaly detection. Other models with a low computational and memory footprint could have been tested, such as one-class SVMs, Isolation Forest or even autoencoders.These models perform well in this type of application and can be easily implemented on a microcontroller.

**Odpověď:**

Please see the response to the previous point: the task after the trigger stage is not anomaly detection but discrimination among impulsive events, where the most dangerous confusers (non-artillery gunfire) are themselves acoustically anomalous with respect to the background. An anomaly detector trained on the ambient distribution would correctly flag a small-arms gunshot as anomalous, which does not provide the artillery/non-artillery decision we need. This motivation for supervised binary classification — and for a CNN over the MFCC tensor as an established compact choice for local spectro-temporal patterns — is now stated explicitly in Section II. We agree that one-class methods are lightweight and MCU-feasible, and we consider them relevant for the first (novelty-detection) stage rather than the second.

### R3.5 Missing train/val/test split, eval on training data

> As regards your results, you are testing your model’s performance on your dataset without first dividing it into a training set, a validation set and a test set. This standard division of the dataset is carried out at random in order to validate the results. We then usually use another dataset to see whether the model has learnt to generalise.
> In this case, your results are not necessarily convincing, as it appears that you are testing your model on the data used to train it.

**Odpověď:**

_(doplnit)_

### R3.6 Three inference regimes unexplained (float32 vs int8 vs ESP32)

> In the results section, you present the performance of your model on a PC using float32, then on a PC using int8 (i.e. after quantisation), and the same quantised model on an ESP32.
> You do not explain why this is the case, nor do you clarify whether it is normal for your model, when quantised on a PC, to perform better than the same model in float32 (which is counterintuitive). The same applies to the results on the ESP32: why does the same model, when run on a different target but using the same dataset, yield different results once again?
> This would warrant further analysis and possible explanations.

**Odpověď:**

_(doplnit)_

### R3.7 Validation on unseen data

> The work presented in the article is, however, interesting, but it should be supplemented and validated using other data that were not encountered during the training.

**Odpověď:**

_(doplnit)_

---

## REVIEW 4 — SCORE: 1 (weak accept), doporučuje major revision

> SUMMARY
> -------
> The paper evaluates the post-trigger classifier of a two-stage acoustic
> artillery-launch detection system. A compact CNN classifies 60 ms peak-centered
> windows as launch vs. non-launch over a 706-event corpus (62 launches / 644
> non-launches). It is tested under added Gaussian noise at SNR = 30/20/10/5 dB in
> three regimes (desktop float32, PC int8, ESP32-S3 int8 via TFLite Micro).
> Headline: MCC 0.99 -> 0.80 across 30 -> 5 dB on ESP32-S3, ~32 ms/segment,
> 80 KiB tensor arena.
>
> STRENGTHS
> ---------
> 1. Clear scoping to the second stage and a candid limitations section.
> 2. Reproducible MFCC front end (framing, hop, window, FFT size, mel/coefficient
>    counts fully specified).
> 3. Concrete deployment evidence: parameter count, arena size, selective operator
>    set, and measured latency.
> 4. Thorough, well-organized related work.

### R4-M1 Three inference regimes disagree

> 1. The three inference regimes disagree with each other. The same quantized
>    model gives materially different results on PC int8 vs. ESP32-S3 (e.g., 30 dB
>    MCC 0.92 vs. 0.99; at 5 dB the precision/recall balance inverts: 0.53/0.90
>    vs. 0.95/0.70). Running one fixed integer model on the same inputs should
>    give essentially identical numbers on both platforms. This discrepancy needs
>    to be explained (identical inputs, scaling, and operator implementations?),
>    because the headline results depend on it.

**Odpověď:**

_(doplnit)_

### R4-M2 "Quantization improves robustness" not established

> 2. The claimed "quantization improves robustness" effect is not established.
>    Going from float32 to int8 raises performance substantially, which is
>    unusual; and if quantization were the cause, the two int8 regimes (concern 1)
>    should agree, but they do not. A simpler explanation - differences in
>    preprocessing or scaling between the float and int8 pipelines - has not been
>    ruled out. This should be investigated before the effect is presented as a
>    robustness benefit rather than reframed as an open observation.

**Odpověď:**

_(doplnit)_

### R4-M3 Headline numbers partly from training data

> 3. The headline robustness numbers are derived from corpus-level stress tests over the full dataset, rather than exclusively from held-out data.
>    The noise stress test runs on all 706 events, most of which were used in
>    training. The abstract and conclusion present the 0.99/0.80 figures without
>    this caveat; results on data not seen during training should be reported and
>    used for the headline claim.

**Odpověď:**

_(doplnit)_

### R4.4 End-to-end dependence on the trigger

> 4. End-to-end dependence on the trigger. Because the 60 ms segments are centered
>    on trigger-detected peaks, all results assume localization already succeeded;
>    end-to-end performance therefore depends on the (out-of-scope, prior-work)
>    triggering stage. Briefly remind the reader in the discussion/conclusion that classifier performance assumes successful trigger localization from the previously validated first stage.

**Odpověď:**

Added as suggested. The discussion (Section VI) now states: "Because each classified segment is centered on a trigger-detected peak, the reported figures characterize classifier performance under the assumption of successful impulse localization by the previously validated first stage." The title has also been narrowed to the post-trigger classifier (see R1.1), so the paper no longer implies end-to-end validation.

### R4.5 Platform motivation

> 5. Platform motivation. State explicitly why ESP32-S3 was chosen (cost, energy,
>    size, or availability).

**Odpověď:**

_(doplnit)_

### R4.6 Front-end placement / purpose of the MCU

> 6. Front-end placement / purpose of the MCU. In the validation protocol, MFCC
>    extraction runs on the host. Clarify the intended deployment partitioning between host and MCU, and whether MFCC extraction is expected to run on-device in the target system. On-device MFCC and end-to-end latency should be quantified in future work.

**Odpověď:**

_(doplnit)_

### R4.7 Dataset details

> 7. Dataset details. State whether the corpus is public; report the launch
>    standoff range/distances and expected operating range; and address behavior
>    across different artillery calibers.

**Odpověď:**

The corpus is not published. Section III.B now reports the measurement geometry: the recording station was positioned approximately 1–2 km from the impact area, and the guns fired from an estimated 5–10 km from the station; launch signatures in the corpus were therefore recorded at a multi-kilometre standoff. Behavior across calibers: the corpus covers a single artillery type (152 mm self-propelled howitzer, stated in Table I), and the limitations section now says explicitly that cross-caliber generalization is untested and remains future work.

### R4.8 Sampling configuration

> 8. Sampling configuration. Briefly justify the choice of 22.05 kHz sampling for the intended deployment scenario.

**Odpověď:**

_(doplnit)_

### R4-minor: Acoustic SNR definition

> - "Acoustic SNR" is undefined - specify the per-event signal/noise power
>   reference and scaling used to set each SNR level.

**Odpověď:**

_(doplnit)_

### R4-minor: Power/energy measurement

> - No power/energy measurement despite the low-power motivation; only latency and
>   memory are reported.

**Odpověď:**

_(doplnit)_

### R4-minor: Figures 1 and 2

> - Figures 1 and 2 are near-duplicative; consider consolidating.

**Odpověď:**

_(doplnit)_

### R4-Q Questions for the authors

> 1. What accounts for the differing PC int8 vs. ESP32-S3 results for the same
>    quantized model?
> 2. Can you rule out preprocessing/scaling differences (rather than quantization)
>    as the source of the float32 -> int8 improvement?
> 3. What are the results on data not used in training?
> 4. How is per-event SNR defined and applied?
> 5. Is the dataset public, and what standoff ranges and calibers does it cover?
> 6. Is "22.05 kHz" the sample rate; if so, why not 44.1 kHz?
> 7. Why ESP32-S3 specifically, and where does MFCC extraction run in the intended
>    deployment?

**Odpověď (Q1–Q7, průřezově — většina otázek se kryje s body výše):**

_(doplnit)_

### R4 Overall assessment

> OVERALL ASSESSMENT
> ------------------
> The deployment contribution is real and clearly presented, and the authors'
> transparency about scope is a strength. However, the robustness claim rests on
> results that disagree across the three inference regimes, an effect
> (quantization) whose cause is not established, and headline numbers measured
> partly on training data. Several system-design and dataset questions (range,
> calibers, sampling rationale, platform motivation, front-end placement, power)
> also remain open.
>
> Recommendation: Major revision. The deployment feasibility result is
> publishable, but the authors should (a) resolve the PC-int8/ESP32 discrepancy,
> (b) report results on data not used in training, (c) explain or reframe the
> quantization effect, and (d) address the dataset/system-design clarifications.

**Odpověď (souhrn k bodům a–d):**

_(doplnit)_

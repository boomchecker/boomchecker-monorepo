# Working memory: source triage for SOTA expansion

Date: 2026-06-01

Scope: triage only. Do not edit `article/article_main.tex` yet. The next step is for the author to locate/store PDFs or authoritative metadata for selected sources, then verify and integrate them.

Inputs reviewed:
- `article/sources/Zdroje pro akustickou detekci střelby.txt`
- `article/sources/zprava-hluboky-vyzkum.md`
- current `article/article_main.tex`
- current `article/references.bib`

Current manuscript shape:
- SOTA already has five slots: classical impulsive detection, artillery/gunfire signature, post-detection classification, robustness/domain shift, embedded deployment.
- Several candidate references are already in `references.bib` but are unused in `article_main.tex`: `crocco2014audio_surveillance`, `gemmeke2017audioset`, `kong2019weakly_audioset`, `kong2020panns`, `lin2020mcunet`, `david2021tflm`, `novac2021quantization_mcu`, `banbury2021micronets`, `gong2021psla`, `hershey2017cnn_audio`.
- The article should avoid expanding into a broad survey. Best use of new sources is to tighten the gap: small-data, post-trigger artillery-launch classification, MFCC/CNN, robustness under controlled perturbation, and TFLite/ESP32-S3 deployment.

## Priority A: find and process first

These are likely useful enough to obtain PDFs/metadata and integrate after verification.

1. TensorFlow Lite Micro: Embedded Machine Learning on TinyML Systems
   - Candidate key already present: `david2021tflm`.
   - Use for: Embedded Deployment and Runtime Constraints; Embedded Inference Protocol.
   - Why: primary/system paper for TFLM, stronger than citing only TensorFlow web documentation.
   - Action: verify venue metadata and replace or supplement generic `tflite` citation in deployment claims.

2. Quantization and Deployment of Deep Neural Networks on Microcontrollers
   - Candidate key already present: `novac2021quantization_mcu`.
   - Use for: embedded deployment, quantization, RAM/latency/energy framing.
   - Why: directly supports "quantization + MCU deployment" without overclaiming quantization as robustness mechanism.
   - Action: use as practical deployment citation, especially if adding model size, arena size, latency, quantization scheme.

3. MCUNet: Tiny Deep Learning on IoT Devices
   - Candidate key already present: `lin2020mcunet`.
   - Use for: resource-constrained neural architecture/inference co-design.
   - Why: good high-level SOTA anchor for TinyML constraints.
   - Action: cite selectively; avoid implying the current model uses NAS/TinyEngine unless true.

4. MicroNets: Neural Network Architectures for Deploying TinyML Applications on Commodity Microcontrollers
   - Candidate key already present: `banbury2021micronets`.
   - Use for: TinyML benchmark/resource constraints.
   - Why: useful for framing that accuracy must be evaluated with memory/latency.
   - Action: cite in embedded constraints if page budget allows.

5. PANNs: Large-Scale Pretrained Audio Neural Networks for Audio Pattern Recognition
   - Candidate key already present: `kong2020panns`.
   - Use for: contrast with large pretrained audio models and transfer-learning literature.
   - Why: helps state why this paper does not use heavy pretrained models: small embedded post-trigger classifier.
   - Action: integrate as contrast, not as direct baseline unless experiments are added.

6. AudioSet: An Ontology and Human-Labeled Dataset for Audio Events
   - Candidate key already present: `gemmeke2017audioset`.
   - Use for: weakly labeled large-scale audio context and transfer-learning datasets.
   - Why: useful if mentioning why public audio pretraining is common but artillery launch data remain narrow/specialized.
   - Action: use only if also citing PANNs/weak labeling; otherwise may be unnecessary.

7. PSLA: Improving Audio Tagging with Pretraining, Sampling, Labeling, and Aggregation
   - Candidate key already present: `gong2021psla`.
   - Use for: modern audio tagging recipes, sampling/labeling/aggregation.
   - Why: relevant to imbalanced audio data, but not central to current method.
   - Action: optional; cite only if the discussion mentions dataset imbalance/training recipes beyond class weighting.

8. Deep Convolutional Neural Networks and Data Augmentation for Environmental Sound Classification
   - Candidate key already used: `salamon2017esc_aug`.
   - Use for: robustness-oriented augmentation in audio.
   - Why: already in text; keep as core augmentation citation.
   - Action: no new PDF needed unless citation audit requires it.

9. Sound of Guns
   - Candidate key already used: `raponi2022sound_of_guns`.
   - Use for: gunshot classification / forensics contrast.
   - Why: directly related to acoustic gunshot classification; already cited.
   - Action: no need to add arXiv version if journal version is already cited.

10. A Survey of Sound Source Localization with Deep Learning Methods
   - Candidate from `zprava-hluboky-vyzkum.md`; not currently in `.bib`.
   - Use for: only if the article discusses localization boundaries.
   - Why: may help clarify that this paper is classification, not localization.
   - Action: lower priority unless SOTA is restructured to include localization as out-of-scope context.

11. Low-Complexity Acoustic Scene Classification with Device Information in the DCASE 2025 Challenge
   - Candidate from `Zdroje pro akustickou detekci střelby.txt`; not currently in `.bib`.
   - Use for: device mismatch, low-complexity acoustic classification, memory/MMAC constraints.
   - Why: highly relevant to domain shift and embedded limits, but it is ASC rather than artillery.
   - Action: verify exact DCASE/arXiv metadata before use; cite as analogous evidence only.

12. Device-Robust Acoustic Scene Classification Based on Two-Stage Categorization and Data Augmentation
   - Candidate from first source file; not currently in `.bib`.
   - Use for: device mismatch, two-stage categorization, augmentation.
   - Why: methodologically close to two-stage audio classification, but not weapon-specific.
   - Action: verify DCASE technical report metadata; use cautiously as challenge/report evidence.

13. Artillery Detection & Localization using a Two-Microphone Array on a Moving Platform
   - Candidate from first source file; not currently in `.bib`.
   - Use for: artillery acoustic localization context.
   - Why: close domain and current/military-relevant; likely useful in SOTA if source quality is acceptable.
   - Action: find PDF, identify whether thesis/report/peer-reviewed publication; cite as technical report only if no formal paper exists.

14. DWT launch/impact artillery classification source
   - Candidate already represented by `desai2006launch_impact`.
   - Use for: classical artillery launch/impact classification and wavelet features.
   - Why: core related work already in text.
   - Action: no new source needed unless DTIC record has more complete metadata than current SPIE entry.

## Priority B: useful only if scope expands

1. Weakly Labelled AudioSet Tagging with Attention Neural Networks
   - Already in `.bib` as `kong2019weakly_audioset`.
   - Use if discussing weak labels or pretraining. Otherwise leave unused.

2. CNN Architectures for Large-Scale Audio Classification
   - Already in `.bib` as `hershey2017cnn_audio`.
   - Use if adding large-scale CNN audio context. Probably optional.

3. Enemy Spotted / BGG dataset
   - Good dataset reference for gunshot classification/localization, but synthetic/in-game domain is far from artillery field recordings.
   - Use only as dataset-availability contrast.

4. Deciphering GunType Hierarchy
   - Current/future-looking gun type classification source.
   - Use only after verifying metadata; likely too far from binary artillery-launch post-trigger classification.

5. End-to-End Environmental Sound Classification using a 1D CNN
   - Use only if explicitly contrasting raw waveform models against MFCC/log-mel features.

6. FSD50K
   - Useful general sound event dataset; not necessary unless adding dataset/pretraining context.

7. FACE active annotation
   - Useful only if manuscript discusses future dataset-building workflow.

8. TinyML book by Warden and Situnayake
   - Practical background, but conference/journal articles are better for SOTA.
   - Use only if venue tolerates book citations and practical implementation framing is needed.

9. DSP / auditory scene analysis books
   - Conceptual background, not needed for a short 5-6 page paper unless adding theory.

10. TinyChirp
   - Nice analogy for low-power always-on acoustic sensing, but bioacoustics is far from artillery. Use only if discussing staged wake-up pipelines.

11. CLAP-S / fiber-optic acoustic recognition
   - Interesting domain adaptation example, but likely too broad unless the domain-shift subsection expands.

## Priority C: avoid unless there is a specific reason

1. Marketing/press sources: Imagimob/Synaptics, PNNL award pages, R&D World, Battle Updates.
   - Problem: not ideal academic evidence for SOTA claims.
   - Use: at most one short industry-context sentence if peer-reviewed sources are unavailable.

2. "Ukraine acoustic weapon" media/video pages.
   - Problem: weak provenance and likely not suitable for scholarly claims.
   - Use: avoid.

3. Generic blog tutorials.
   - Problem: not scholarly evidence.
   - Use: avoid.

4. Audio LLM / backdoor / Eureka-Audio sources.
   - Problem: far from embedded artillery-launch post-trigger classification.
   - Use: avoid in current paper; it would broaden SOTA without helping the gap.

5. LLM-specific or vision-transformer-specific quantization papers.
   - Problem: method domain mismatch.
   - Use: avoid unless the paper adds a careful, narrow "quantization can affect robustness" discussion.

6. Medium article "Why Your Quantized Model Might Outperform..."
   - Problem: blog source; not suitable as evidence.
   - Use: avoid.

7. Claims that quantization is an adversarial shield or causal regularizer.
   - Problem: current manuscript does not isolate causality. Strong claims would invite reviewer pushback.
   - Use: keep wording empirical: quantized inference performed better in this experiment; mechanism remains unisolated.

## Recommended integration plan after PDFs are added

1. Tighten SOTA rather than expand it:
   - artillery/mortar acoustic sensing;
   - impulsive audio classification features and CNNs;
   - robustness/domain shift/device mismatch;
   - embedded TinyML/TFLM deployment;
   - narrow gap statement.

2. Use existing `.bib` entries first:
   - `david2021tflm`, `novac2021quantization_mcu`, `lin2020mcunet`, `banbury2021micronets`, `kong2020panns`, `gemmeke2017audioset`, `gong2021psla`.

3. Add only a small number of new `.bib` entries:
   - DCASE 2025 low-complexity ASC with device information.
   - DCASE 2020 device-robust two-stage ASC if verified.
   - Aalborg artillery localization report if source quality is acceptable.
   - Grumiaux localization survey only if localization needs an explicit out-of-scope citation.

4. Do not add broad quantization-regularization literature unless the manuscript adds a careful limitation paragraph. The current evidence supports deployment behavior and PC-side int8/ESP32-S3 consistency, not a general causal theory.

## First question to resolve before writing

Question: Should the SOTA expansion emphasize "embedded deployment/TinyML" or "robustness/domain shift" as the primary missing literature axis?

Recommended answer: prioritize embedded deployment/TinyML first, then add one compact robustness/domain-shift paragraph. Reason: the article's distinctive contribution is not a new audio model, but showing a compact post-trigger artillery-launch classifier behaving well through int8/TFLM/ESP32-S3 deployment under controlled perturbation.

Decision from author, 2026-06-01:
- Primary SOTA expansion axis: embedded deployment / TinyML.
- Robustness and domain shift should remain a secondary support axis.
- Practical implication: prioritize sources on TFLM, MCU deployment, quantization on microcontrollers, memory/latency/resource constraints, and compact edge audio inference. Avoid broadening into a general robustness/domain-adaptation survey.

Second framing decision from author, 2026-06-01:
- Frame the paper more broadly as a two-stage embedded acoustic pipeline:
  1. streaming/event trigger stage for candidate impulsive events,
  2. slower post-trigger CNN classifier for artillery-launch filtering.
- Keep the claim boundary explicit: this manuscript experimentally validates the second stage, especially int8/TFLM classifier behavior on ESP32-S3 using host-computed MFCC tensors.
- It is acceptable and desirable to mention that acquisition and trigger components exist and have been tested elsewhere, but they should not be presented as newly evaluated end-to-end evidence in this paper unless new measurements are added.
- Wording target: "embedded two-stage pipeline with experimentally validated post-trigger classifier deployment", not "fully validated end-to-end embedded detector".

Third scope/terminology decision from author, 2026-06-01:
- Primary application and terminology: artillery launch detection / artillery gunshot classification.
- General gunshot/firearm classification should be mentioned only lightly and only where it supports methods, feature choices, or nearby audio-classification context.
- Do not let generic firearm recognition sources dominate SOTA. They are related work, not the target problem.
- Practical implication: prioritize artillery/mortar launch-impact classification, artillery acoustic localization, impulsive military acoustic sensing, and embedded deployment sources over handgun/firearm forensics datasets.

Fourth scope decision from author, 2026-06-01:
- Keep the manuscript focused on classification, not localization.
- Artillery acoustic localization may appear only as minimal context, ideally no more than one or two sentences.
- Do not add a dedicated localization SOTA branch.
- Do not prioritize new localization sources unless they also directly support artillery launch classification or embedded deployment framing.

Fifth integration decision from author, 2026-06-01:
- Embedded SOTA emphasis: primarily TensorFlow Lite Micro / MCU inference frameworks.
- Quantization may be added secondarily if the selected sources directly support it.
- Do not expand SOTA substantially. Target: smart citation strengthening and at most one additional paragraph, preferably a careful extension of the existing embedded deployment subsection.
- Preferred wording: quantization as an embedded deployment enabler and possible empirical contributor to robustness, not a broad causal regularization theory.
- Practical implication: first use `david2021tflm` and `novac2021quantization_mcu`; optionally support with `lin2020mcunet` / `banbury2021micronets` if space allows. Avoid long detours into QAT/adversarial robustness papers.

## Download shortlist for author

Download first:
1. David et al. - TensorFlow Lite Micro: Embedded Machine Learning on TinyML Systems
   - Priority: essential.
   - Reason: primary TFLM / MCU inference framework source.
   - Current `.bib` key: `david2021tflm`.

2. Novac et al. - Quantization and Deployment of Deep Neural Networks on Microcontrollers
   - Priority: essential.
   - Reason: practical MCU quantization/deployment source; supports memory, latency, and inference-engine framing.
   - Current `.bib` key: `novac2021quantization_mcu`.

3. Lin et al. - MCUNet: Tiny Deep Learning on IoT Devices
   - Priority: high, but secondary.
   - Reason: strong embedded/TinyML co-design context.
   - Current `.bib` key: `lin2020mcunet`.

4. Banbury et al. - MicroNets: Neural Network Architectures for Deploying TinyML Applications on Commodity Microcontrollers
   - Priority: high, but secondary.
   - Reason: TinyML model/resource-constraint context.
   - Current `.bib` key: `banbury2021micronets`.

Download if easy / if space remains:
5. DCASE 2025 - Low-Complexity Acoustic Scene Classification with Device Information
   - Priority: optional.
   - Reason: useful for low-complexity audio classification under device mismatch and strict resource constraints.
   - Not currently in `.bib`.
   - Use carefully as analogous acoustic-scene evidence, not artillery evidence.

6. DCASE 2020 - Device-Robust Acoustic Scene Classification Based on Two-Stage Categorization and Data Augmentation
   - Priority: optional.
   - Reason: supports two-stage audio classification and device-robust framing.
   - Not currently in `.bib`.
   - Use carefully as analogous ASC evidence.

7. TinyChirp: Bird Song Recognition Using TinyML Models on Low-power Wireless Acoustic Sensors
   - Priority: optional / low.
   - Reason: useful analogy for low-power staged acoustic sensing, but bioacoustics is not artillery.
   - Use only if we need one supporting example of always-on low-power acoustic TinyML.

Probably do not download now:
8. General gunshot/firearm datasets and classifiers: Enemy Spotted, Deciphering GunType Hierarchy, generic autonomous-vehicle gunshot class paper.
   - Reason: current author decision is to keep scope on artillery launch classification.

9. Localization-only sources: Grumiaux localization survey, two-microphone artillery localization report.
   - Reason: current author decision is classification-only; localization gets at most one or two context sentences.

10. Broad quantization/adversarial robustness papers: Quant-Noise, ACosR, QANS, ERQ, RVQ perturbation, deeply quantized adversarial networks, Medium/blog sources.
   - Reason: too broad and risks overclaiming a causal robustness mechanism.

## New PDF extraction notes, 2026-06-01

Raw text extracted with `pdftotext -layout -enc UTF-8` for:
- `3495724.3496706.txt`
- `DCASE2020_Hu_114.txt`
- `DCASE2025Workshop_Schmid_3.txt`
- `MLSys-2021-micronets-neural-network-architectures-for-deploying-tinyml-applications-on-commodity-microcontrollers.txt`
- `MLSys-2021-tensorflow-lite-micro-embedded-machine-learning-for-tinyml-systems-Paper.txt`
- `Rigorous_Floating-Point_to_Fixed-Point_Quantization_of_Deep_Neural_Networks_on_STM32_Micro-controllers.txt`
- `TinyChirp_Bird_Song_Recognition_Using_TinyML_Models_on_Low-power_Wireless_Acoustic_Sensors.txt`

MiKTeX reported denied access to its own `pdftotext.log`, but the extracted text files were created successfully.

### Source quality / integration matrix for newly extracted PDFs

| PDF | Identified source | Current bib status | Best use in article | Recommendation |
|---|---|---|---|---|
| `MLSys-2021-tensorflow-lite-micro-embedded-machine-learning-for-tinyml-systems-Paper.pdf` | David et al., "TensorFlow Lite Micro: Embedded Machine Learning on TinyML Systems", MLSys 2021 | Already in `.bib` as `david2021tflm` | Primary support for TFLM as MCU inference framework; static memory arena; selective operator resolver; resource-constrained deployment | Use. This should replace/supplement generic `tflite` web citation in embedded deployment claims. |
| `MLSys-2021-micronets...pdf` | Banbury et al., "MicroNets...", MLSys 2021 | Already in `.bib` as `banbury2021micronets` | TinyML models must satisfy MCU memory, latency, and energy constraints; TFLM deployment and TinyMLPerf audio keyword spotting/anomaly tasks | Use selectively. Good second citation in embedded constraints paragraph. |
| `3495724.3496706.pdf` | Lin et al., "MCUNet: Tiny Deep Learning on IoT Devices", NeurIPS 2020 | Already in `.bib` as `lin2020mcunet` | General MCU memory/storage constraint framing; model/inference co-design; audio wake-word evidence | Optional. Good for a broad resource-constraint sentence, but do not imply this manuscript uses NAS or TinyEngine. |
| `Rigorous_Floating-Point_to_Fixed-Point_Quantization...pdf` | Ben Khalifa and Martel, "Rigorous Floating-Point to Fixed-Point Quantization of Deep Neural Networks on STM32 Micro-controllers", CoDIT 2024, DOI in PDF metadata: `10.1109/CoDIT62066.2024.10708400` | Not currently in `.bib`. Important: this is not `novac2021quantization_mcu`. | Fixed-point/quantized deployment on STM32-class MCUs; time/energy/memory motivations | Optional and cautious. Only add if we want a newer fixed-point MCU citation. Existing article can already cite `jacob2018quantization` and `novac2021quantization_mcu`; if using this PDF, add a new bib entry. |
| `DCASE2025Workshop_Schmid_3.pdf` | Schmid et al., "Low-Complexity Acoustic Scene Classification with Device Information in the DCASE 2025 Challenge" | Not currently in `.bib` | Analogous evidence that embedded audio classification literature now treats low complexity, device mismatch, data efficiency, and transfer learning as deployment constraints; includes 128 kB / 30 MMAC constraints for one-second audio segment | Optional. Useful only for one compact robustness/device-mismatch sentence. Do not make it central because it is ASC, not artillery. |
| `DCASE2020_Hu_114.pdf` | Hu et al., "Device-Robust Acoustic Scene Classification Based on Two-Stage Categorization and Data Augmentation", DCASE 2020 technical report | Not currently in `.bib` | Analogous two-stage ASC, data augmentation for device robustness, TensorFlow Lite dynamic range quantization under 500 KB model-size constraint | Low priority. It supports "two-stage audio classification + augmentation + quantization" but is a challenge technical report and scene classification, not artillery. |
| `TinyChirp...pdf` | Huang et al., "TinyChirp: Bird Song Recognition Using TinyML Models on Low-power Wireless Acoustic Sensors", IEEE IS2 2024, DOI in PDF metadata: `10.1109/IS262782.2024.10704131` | Not currently in `.bib` | Low-power acoustic sensors, pre-screening/classification on MCU nodes, memory/time/energy profiling, two-stage binary classification | Usually skip. Strong TinyML acoustic-sensor analogy, but bioacoustics is far from artillery and would broaden SOTA. Use only if the editor/reviewer needs another real acoustic-sensor deployment example. |

### Claim anchors from the extracted PDFs

TFLM / `david2021tflm`:
- TFLM is explicitly presented as an open-source inference framework for deep-learning models on embedded systems.
- It targets resource constraints and embedded-platform fragmentation.
- It uses an interpreter-based design with low runtime overhead in evaluated TinyML workloads.
- Its application flow uses an operator resolver and a contiguous memory arena; memory is allocated during initialization to avoid later dynamic allocation. This directly supports the article's `MicroMutableOpResolver` and `tensor_arena` discussion.

MicroNets / `banbury2021micronets`:
- TinyML inference on MCUs is framed around memory, latency, and energy constraints.
- MicroNets are deployed using TensorFlow Lite Micro.
- The work treats operation count as a useful latency proxy over NAS search spaces and reports TinyMLPerf tasks including audio keyword spotting and anomaly detection.
- Useful as context that deployable embedded audio models should be discussed together with resource budgets, not just accuracy.

MCUNet / `lin2020mcunet`:
- MCU memory is described as 2-3 orders of magnitude smaller than mobile phones; the paper gives an example of 320 kB SRAM and 1 MB Flash on STM32F746.
- Even int8 MobileNetV2 can exceed small MCU memory limits in their example.
- TinyNAS/TinyEngine co-design reduces peak memory and improves inference speed, including visual/audio wake-word tasks.
- Use as background on why compact model/resource co-design matters. Avoid implying that this manuscript uses MCUNet's methods.

Ben Khalifa and Martel 2024:
- Focuses on floating-point to fixed-point conversion for DNNs on low-power 32-bit STM32 microcontrollers.
- Motivates quantization by memory footprint, real-time, energy, and edge deployment constraints.
- Reports lower time and energy consumption for fixed-point networks than floating-point versions on ARM microcontroller evaluation.
- Not a direct source for "quantization improves robustness"; only supports deployment efficiency and fixed-point conversion.

DCASE 2025 low-complexity ASC:
- Recent DCASE ASC tracks emphasize low-complexity constraints, recording-device mismatch, and data efficiency.
- The 2025 baseline uses a two-stage training procedure: general model first, then device-specific adaptation.
- Complexity limits include 128 kB model parameters and 30 MMACs per one-second audio segment, targeting Cortex-M4-class devices.
- Use only as analogous audio-deployment evidence; not as artillery evidence.

DCASE 2020 device-robust ASC:
- Technical report for DCASE 2020 Task 1.
- Proposes a two-stage ASC system combining three-class and ten-class CNN classifiers.
- Uses data augmentation to reduce device dependency.
- Uses TensorFlow Lite dynamic range quantization to reduce model size to roughly one eighth, reporting only small accuracy degradation in their task.
- Lower priority because it is a technical report and task-specific ASC.

TinyChirp:
- Studies low-power wireless acoustic sensors using TinyML for bird-song recognition.
- Frames continuous recording as a memory/energy bottleneck and proposes on-device pre-screening/classification.
- Reports precision over 0.98 and battery-life extension from roughly two weeks to eight weeks in their use case.
- Strong analogy for low-power acoustic sensing, weak domain match for artillery.

### Integration implication after extraction

Best minimal change path:
1. In `State of the Art -> Embedded Deployment and Runtime Constraints`, replace the generic TFLite-only framing with `david2021tflm` as the primary TFLM systems citation.
2. Add one sentence using `lin2020mcunet` and/or `banbury2021micronets` to support the statement that MCU deployment must account for memory, latency, energy, and runtime/operator overhead, not just model accuracy.
3. Keep quantization wording narrow: cite `jacob2018quantization` for integer-only inference and optionally `novac2021quantization_mcu` or Ben Khalifa and Martel 2024 for MCU deployment efficiency. Do not claim quantization causally improves robustness.
4. Do not add DCASE 2020, DCASE 2025, or TinyChirp unless a single bridging sentence is needed for device mismatch / low-complexity acoustic classification. They are useful analogies, not core artillery-launch literature.

Potential compact paragraph direction:
"Embedded audio classifiers are increasingly evaluated under joint accuracy and resource constraints. TFLM provides a portable MCU inference runtime with static memory planning through a user-supplied arena and selective operator registration. TinyML work such as MCUNet and MicroNets shows that memory, latency, and energy constraints can dominate architecture choices on commodity MCUs. In this context, our ESP32-S3 evaluation reports not only classification metrics but also the inference regime, 80 KB tensor arena, and observed per-segment latency."

Need to convert this direction into final IEEE prose only after author confirms whether DCASE/TinyChirp analogies should be excluded entirely or allowed as one optional bridging sentence.

Author decision, 2026-06-01:
- Exclude DCASE 2020, DCASE 2025, and TinyChirp from the manuscript unless a later reviewer/editorial need creates a specific reason.
- Do not add citations just to increase citation count.
- Integration style should be surgical: cite only sources that directly support a concrete sentence in the article.
- The STM32 quantization PDF should be available under a new `.bib` key if needed, but it should be used only for fixed-point/quantized MCU deployment efficiency, not as evidence that quantization causally improves robustness.
- Novac et al. 2021 was not the newly downloaded PDF and should not be treated as extracted source evidence.

Updated citation candidates after author decision:
1. `david2021tflm` - primary support for TFLM, memory arena, operator resolver, embedded-runtime framing.
2. `banbury2021micronets` - secondary support for MCU memory/latency/energy constraints and TinyML deployment benchmarks.
3. `lin2020mcunet` - optional support for general MCU memory/storage constraints and model/runtime co-design.
4. `benkhalifa2024stm32_quantization` - optional support for fixed-point/quantized DNN deployment on STM32-class MCUs.
5. `jacob2018quantization` - general integer-only inference citation; keep for the method/deployment protocol if the wording remains broad.

Second grill-me decision from author, 2026-06-01:
- Keep the `tflite` web citation if it is used as practical TensorFlow Lite framework documentation.
- For more scientific / systems claims about TFLM itself, change or supplement the citation with `david2021tflm`.
- Practical implication: do not remove `tflite` mechanically. Use `david2021tflm` for claims about TFLM design, embedded runtime constraints, static memory arena, operator resolver, and low-overhead MCU inference.

STM32 quantization fit decision, 2026-06-01:
- Reviewed Ben Khalifa and Martel 2024 beyond metadata.
- Fit: narrow but useful for fixed-point / quantized DNN deployment on low-power STM32-class MCUs. The paper motivates quantization through memory footprint, real-time constraints, energy use, and edge deployment, and reports lower execution time and energy for generated fixed-point networks than floating-point versions on STM32 Nucleo-144.
- Limits: it uses a formal Popinns floating-point-to-fixed-point workflow, small benchmark networks, and STM32 rather than ESP32-S3/TFLite Micro. It does not support any causal claim that quantization improves robustness.
- Integration decision: cite it only in the general embedded deployment / quantization framing, not in Results or Discussion as an explanation of the observed MCC differences.

Final SOTA integration pass, 2026-06-01:
- `article/article_main.tex` was updated only in the embedded/TinyML deployment path.
- Added a compact SOTA sentence citing `lin2020mcunet` and `banbury2021micronets` for MCU memory, latency, energy, and runtime-overhead constraints.
- Kept `tflite` as framework documentation and paired it with `david2021tflm` where the text makes systems-level claims about TensorFlow Lite Micro.
- Used `benkhalifa2024stm32_quantization` only as support for fixed-point / quantized MCU deployment efficiency, alongside `jacob2018quantization`.
- Refined the embedded inference protocol citation so static arena / selective operator registration is supported by `david2021tflm`, while int8 inference remains supported by `jacob2018quantization`.
- Did not add DCASE 2020, DCASE 2025, or TinyChirp to the manuscript.

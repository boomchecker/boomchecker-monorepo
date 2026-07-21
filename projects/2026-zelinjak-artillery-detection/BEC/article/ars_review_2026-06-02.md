# ARS peer review aktualniho rukopisu

- Datum review: 2026-06-02
- Workflow: `academic-research-suite` -> `academic-paper-reviewer` -> `full`
- Reviewed file: `article/article_main.tex`
- Review round: pre-submission internal review
- Celkovy verdikt: `Major Revision`
- Confidence: `4/5`

Poznamka k overeni: Review vychazi z lokalniho rukopisu a lokalniho `references.bib`. Externi DOI, bibliograficka metadata a aktualnost literatury jsem v tomto kole neoveroval webem. Komentare k citacim jsou proto vecne a argumentacni, ne formalni citation audit.

## Editorial decision

**Decision: Major Revision before submission.**

Rukopis je proti starsi verzi podstatne lepe vymezeny. Uz netvrdi, ze validuje cely continuous-stream detektor; opakovane omezuje scope na post-trigger classifier, rozlisuje MFCC-domain nominal SNR od waveform-domain acoustic SNR a priznava, ze quantization effect neni kauzalne izolovany. To jsou silna zlepseni.

Soucasne vsak zustavaji dva blokujici problemy. Prvni je experimentalni interpretace: stejne kvantizovany classifier ma podle textu bezet na PC int8 i ESP32-S3, ale embedded vysledky jsou vyrazne lepsi nez PC int8 vysledky, zejmena v precision a MCC. Bez vysvetleni inference path, thresholdingu, quantization scalingu, vstupnich tensoru a denominatoru to vypada jako neekvivalentni evaluace, nikoli jako deployment validation. Druhy problem je statisticka prukaznost: 62 pozitivnich launch eventu a jeden 80/20 split jsou pro tvrzeni o robustnosti a embedded stability krehke, pokud nejsou dodany absolutni confusion matrices, pocty evaluacnich pripadu, confidence intervals nebo repeated/grouped split.

Clanek ma publikovatelne jadro, ale pred odeslanim musi uzavrit validacni protokol a zmirnit tvrzeni, ktera presahuji evidenci. Nejblizsi vhodny framing je: **empirical validation of a quantized post-trigger MFCC-CNN classifier for artillery launch events under controlled feature-space perturbation on ESP32-S3**, ne plna system-level field robustness.

## Field analysis

| Dimension | Analysis |
|---|---|
| Primary discipline | Applied acoustic sensing / acoustic event classification |
| Secondary disciplines | TinyML, embedded inference, defense sensing, audio signal processing |
| Research paradigm | Quantitative experimental ML evaluation |
| Methodology type | Supervised binary classification, perturbation robustness test, quantized embedded inference validation |
| Target venue tier | Q2/Q3 specialized embedded sensing / applied acoustics / defense technology venue in current form; stronger venue only after more robust validation |
| Paper maturity | Revised draft, but not submission-ready because the main evidence chain is not yet fully reproducible |

## Reviewer configuration

1. **EIC**: Editor for an applied embedded sensing or measurement systems venue. Focus: contribution, scope discipline, reader interest, fit between evidence and claims.
2. **Reviewer 1, Methodology**: ML-for-audio experimentalist familiar with small imbalanced datasets, robustness testing, quantization, and reproducibility. Focus: split design, leakage, metrics, uncertainty, inference equivalence.
3. **Reviewer 2, Domain**: Researcher in acoustic detection of impulsive weapon-related events. Focus: artillery/gunfire acoustics, two-stage detection logic, literature positioning, terminology.
4. **Reviewer 3, Perspective**: Embedded systems / operational deployment reviewer. Focus: practical deployability, host-device protocol, latency/memory reporting, system integration.
5. **Devil's Advocate**: Adversarial reviewer testing whether the core conclusion follows from the reported tables.

## Executive findings

### CRITICAL

1. **PC int8 vs ESP32-S3 equivalence is not demonstrated.** The paper states that the ESP32-S3 runs the same quantized post-trigger classifier and receives host-computed MFCC tensors under the same perturbation settings (`article_main.tex`, lines 155-162). Yet Table 2 reports much stronger ESP32-S3 results than PC int8, for example precision 0.53 -> 0.95 and MCC 0.6511 -> 0.8009 at 5 dB (lines 202-210). If the model, input tensors, scaling, threshold, and labels are identical, this gap needs a concrete technical explanation. Without it, the main deployment conclusion is not secure.

2. **Metric denominators and evaluation units are under-specified.** The dataset has 706 clean events and 62 positives (lines 48, 122-124, 157-158). A single stratified 80/20 split implies roughly 141-142 clean test events and about 12-13 positives before any corruption expansion. Reported accuracies such as 99.86% and 99.73% imply either a larger evaluation denominator, averaging over corrupted variants, repeated stochastic perturbations, or some aggregation not described in the manuscript. The paper must state the exact number of evaluated items for each row.

### MAJOR

3. **Single event-level split is too weak for the robustness claim.** The paper explicitly uses one stratified event-level 80/20 split (line 158) and later acknowledges this limitation (line 219). For acoustic field data, event-level split may still leak recording-session, weapon, day, geometry, or environment context. A grouped split by session/scenario, or at least a repeated split with confidence intervals, is needed if the paper wants to claim robustness rather than a promising pilot result.

4. **Thresholding and model selection are missing.** The classifier ends in a sigmoid (line 137), but the manuscript does not specify whether the decision threshold is fixed at 0.5, selected on a validation set, transferred unchanged across float32/int8/ESP32, or calibrated separately. It also says "selected model" (line 155) without explaining validation, early stopping, checkpoint choice, or representative dataset choice for quantization.

5. **Quantization benefit is framed cautiously but still over-inferred.** The paper says the mechanism is not isolated (lines 188, 219), which is good. But the discussion and conclusion still lean on the idea that quantized inference is "markedly more robust" (lines 217, 224). That may be true, but it currently lacks ablations separating quantization, thresholding, representative calibration data, input scaling, and runtime implementation differences.

6. **Feature-space SNR is acknowledged but not fully operationalized.** The manuscript correctly says MFCC-domain SNR is not waveform-domain SNR (lines 149, 170, 194, 219). However, it does not give the formula for converting desired SNR to Gaussian perturbation variance, nor whether power is computed per sample, per coefficient, per corpus, before/after normalization, or using train-only statistics.

7. **Dataset description is still too thin for field acoustic work.** Lines 108-111 identify microphones/interface and class counts, but do not describe number of sessions, artillery types, distances, geometries, environmental conditions, source of non-launch negatives, label protocol, or whether launch and impact events from the same firing sequence can cross split boundaries.

### MINOR

8. **Title slightly overpromises "pipeline."** The paper is clear internally that it studies the post-trigger classifier (lines 48, 60, 64, 97), but the title line 17 still advertises a "Two-Stage Acoustic Pipeline." A stricter title should name the classifier stage or embedded post-trigger validation.

9. **Resource reporting should be expanded.** Latency and tensor arena are useful (line 191), but embedded readers will expect model size, flash/RAM footprint, operator list, CPU frequency, measurement protocol, number of latency trials, and whether 32 ms excludes host-side MFCC computation and UART/transport overhead.

10. **Results would be easier to trust with absolute counts.** Tables 1 and 2 should include or be accompanied by TP, FP, TN, FN for each row. This is especially important because the float32 model has high recall but poor precision (lines 167-181).

## EIC review report

### Recommendation

`Major Revision`

### Confidence

`4/5`

### Summary assessment

The paper addresses a relevant applied problem: lightweight post-trigger classification of artillery launch events on constrained embedded hardware. Its strongest editorial quality is scope discipline. The manuscript repeatedly clarifies that it evaluates the second-stage classifier rather than the full streaming detector, and it distinguishes feature-space perturbation from acoustic waveform SNR. This makes the contribution more credible than a broader "field robust detector" claim would be.

The manuscript is not yet submission-ready because the strongest result is internally hard to interpret. The ESP32-S3 deployment appears substantially better than PC-side int8 verification despite being described as the same quantized classifier under the same host-generated inputs. A reader will not accept this as deployment stability until the authors show equivalence of input tensors, quantization scales, thresholding, and evaluation units. I recommend major revision rather than rejection because the issue is likely fixable through clearer protocol, additional tables, and possibly rerunning a targeted equivalence check.

### Strengths

1. **Scope is much cleaner than a full-system claim.** Lines 48, 60, 64, and 97 consistently identify the evaluated object as the post-trigger classifier.
2. **Limitations are not hidden.** Line 219 explicitly notes no continuous-stream false-trigger evaluation, single split, MFCC-space perturbation, non-isolated quantization mechanism, small positive class, and single target hardware.
3. **The paper reports useful metrics beyond accuracy.** Precision, recall, F1, and MCC are reported in lines 176-181 and 200-210, which is appropriate for a 62/644 class imbalance.

### Weaknesses

1. **Core deployment conclusion is not yet sufficiently evidenced.** The abstract and conclusion cite ESP32-S3 MCC values as strong support (lines 48, 224), but the bridge between PC int8 and ESP32-S3 needs explanation.
2. **Contribution framing is still slightly stronger than the measured scope.** The title and some conclusion language imply a pipeline contribution, while the evaluated experiment is only classifier-stage validation.
3. **Venue fit depends on missing deployment details.** Embedded sensing readers need model size, RAM/flash footprint, latency measurement setup, and feature-extraction boundary conditions.

### Questions for authors

1. Are PC int8 and ESP32-S3 evaluated on bit-identical MFCC tensors with the same quantization parameters and the same decision threshold?
2. What exactly is the evaluation denominator for each row in Table 2?
3. Is the ESP32-S3 result computed from labels returned by the device, raw scores returned by the device and thresholded on host, or a hybrid process?

## Methodology review report

### Recommendation

`Major Revision`

### Confidence

`5/5`

### Summary assessment

The method is plausible but not reproducible enough for the claims being made. The feature pipeline, network topology, and basic training ingredients are described, but the evaluation protocol leaves out the details that determine whether the results are valid: grouping, thresholding, model selection, quantization calibration, exact perturbation generation, and per-row denominators. The most serious issue is the unexplained gap between PC int8 and ESP32-S3 metrics. The second most serious issue is relying on one event-level split with only 62 positives.

### Methodological strengths

1. **Leakage from augmentation siblings is addressed at the event level.** Line 158 says the split is performed before train-time augmentation.
2. **The paper uses MCC.** This is a good choice for the 8.8% positive rate (lines 122-124, 157-158).
3. **MFCC-space perturbation is not mislabeled as acoustic SNR.** This is repeatedly clarified in the abstract, methods, tables, and limitations.

### Methodological weaknesses

1. **Inference equivalence is not proven.** Add a row-level equivalence table: same model file/hash, same representative dataset, same input quantization scale/zero point, same output dequantization, same threshold, same test tensors, same labels, same random seeds. Also report maximum absolute difference between PC int8 outputs and ESP32 outputs for a fixed set of samples.

2. **No uncertainty estimation.** Add 95% confidence intervals for MCC, recall, precision, and F1. With small positive counts, bootstrap over events or repeated grouped splits would be more convincing than a single split.

3. **No grouped validation.** If metadata exist, split by recording session, day, position, weapon, or firing sequence. If not, say explicitly that grouped validation was impossible and discuss the resulting optimism risk.

4. **Perturbation formula is missing.** Define the Gaussian perturbation as an equation. State whether sigma is derived from MFCC matrix power, coefficient-wise standard deviation, global train-set statistics, or another convention. State whether perturbations are regenerated per run.

5. **No threshold protocol.** State the decision threshold and how it was selected. If threshold differs between float32, PC int8, and ESP32-S3, the comparison is no longer a pure quantization/deployment comparison.

6. **Class weighting is not reproducible.** Give the exact class weights and whether weights are computed from the clean train split, augmented train split, or another distribution.

### Required methodological additions

- Confusion matrices for every table row.
- Evaluation counts for every table row: number of clean events, number of corrupted variants, number of positive and negative evaluated instances.
- Exact split counts: train/test positives and negatives before and after augmentation.
- Quantization details: representative dataset, integer type, per-tensor/per-channel settings if applicable, input/output scale and zero point.
- Latency protocol: sample size, mean/median/std, clock frequency, what is included/excluded.

## Domain review report

### Recommendation

`Major Revision`

### Confidence

`4/5`

### Summary assessment

The state of the art is reasonably targeted. It covers classical acoustic triggering, artillery/gunfire variability, MFCC/CNN classification, feature-domain augmentation, and TinyML deployment constraints. The manuscript also makes a useful distinction between launch classification and impact modeling. The domain weakness is not mainly missing literature; it is that the dataset and operational context are under-described relative to the field claim. In artillery acoustics, geometry, propagation, firing sequence structure, and environmental conditions are not metadata trivia. They determine whether a classifier has learned launch acoustics or recording-context artifacts.

### Strengths

1. **Good two-stage framing.** Lines 58, 68, and 97 correctly separate event triggering from semantic classification.
2. **Acoustic variability is acknowledged.** Line 71 notes muzzle blast, shock wave, delayed arrivals, terrain, atmosphere, and sensor geometry.
3. **Feature-domain robustness is positioned with caveats.** Line 88 gives a defensible rationale while recognizing non-equivalence to waveform noise.

### Weaknesses

1. **Operational dataset context is insufficient.** Add table or paragraph for sessions, distances, sensor placement, weapons/source types if discloseable, environment, and negative-event categories.
2. **Launch vs impact handling needs clearer split logic.** Line 109 says recordings include launch and impact events but only launch is positive. Clarify whether impact events are in the negative class, excluded, or future work only.
3. **No baseline anchors.** A compact CNN may be reasonable, but the paper would be stronger with a simple baseline such as logistic regression/SVM/random forest on the same MFCCs, or a tiny MLP. Without it, the architecture contribution is hard to evaluate.
4. **Trigger dependency is treated as prior validation but still affects interpretation.** Since the post-trigger window depends on peak-centered extraction (line 99), describe how close this is to the actual streaming trigger output and whether trigger timing error was simulated.

### Recommended domain additions

- Dataset context table.
- Negative class taxonomy.
- Clear statement on impact-event inclusion/exclusion.
- Short baseline comparison or a rationale for why baseline is omitted.
- One paragraph explaining how much timing jitter the classifier can tolerate, or naming it as future work.

## Perspective review report

### Recommendation

`Major Revision`

### Confidence

`4/5`

### Summary assessment

From an embedded deployment perspective, the manuscript is promising but still closer to a hardware-in-the-loop classifier validation than to an operational embedded acoustic system. This is acceptable if framed directly. The text already says host-device transfer is a validation protocol rather than the intended operational pipeline (line 155), which is exactly the right caveat. The revision should build around that honesty: present ESP32-S3 as evidence that the classifier inference stage is feasible, not that the complete acoustic detector has been embedded end to end.

### Strengths

1. **The host-device boundary is disclosed.** Line 155 states that the host computes MFCC tensors and sends them to the embedded target.
2. **Resource signals are included.** Line 191 reports 32 ms latency and 80 KB tensor arena.
3. **ESP32-S3 is framed as representative rather than universal.** Line 155 avoids overgeneralizing to all embedded hardware.

### Weaknesses

1. **Deployment cost is incomplete.** End-to-end embedded feasibility depends on MFCC extraction, buffering, trigger, I/O, memory, power, and false-trigger rate, not only CNN inference.
2. **The latency number is underspecified.** State whether 32 ms is average over N samples, whether it includes tensor copy and quantization, and what CPU frequency/configuration was used.
3. **The paper needs a system boundary diagram.** A small figure or table separating "validated here", "previously validated", and "future work" would prevent reviewers from attacking the pipeline claim.

## Devil's Advocate review

### Strongest counter-argument

The strongest skeptical reading is that the paper's central result may be an artifact of non-equivalent evaluation paths. The authors claim that quantized inference is more robust and that ESP32-S3 deployment preserves high performance. However, if the same quantized model receives the same MFCC tensors, the embedded device should not dramatically outperform the PC int8 reference. The reported gap, especially precision 0.53 versus 0.95 at 5 dB, is too large to treat as harmless deployment noise. A reviewer could argue that the experiment is currently comparing different thresholds, different quantization/dequantization behavior, different input sets, different denominator definitions, or different post-processing. If so, the paper does not yet validate embedded robustness; it only reports a set of results whose evaluation equivalence is unclear. The paper may still contain a useful system engineering result, but the main empirical claim needs an audit trail before it can carry the conclusion.

### Issue list

#### CRITICAL

| # | Dimension | Issue | Location |
|---|---|---|---|
| DA-C1 | Data-conclusion mismatch | ESP32-S3 is claimed not to show harmful drift relative to PC int8, but it is substantially better than PC int8 without explanation. | Lines 188-191, 202-210 |
| DA-C2 | Evidence chain break | Per-row denominators are absent, so the precision of the percentages and MCC cannot be interpreted. | Lines 157-158, 202-210 |

#### MAJOR

| # | Dimension | Issue | Location |
|---|---|---|---|
| DA-M1 | Overgeneralization | "Pipeline" language is broader than the evaluated post-trigger classifier stage. | Lines 17, 48, 97 |
| DA-M2 | Hidden assumption | Event-level split is assumed sufficient for field data, but session/scenario correlation is not discussed. | Lines 108-111, 158 |
| DA-M3 | Alternative explanation | Quantization may not be the cause of the gain; thresholding, calibration, or evaluation differences could explain it. | Lines 188, 217-224 |

#### MINOR

| # | Dimension | Issue | Location |
|---|---|---|---|
| DA-m1 | Framing | The conclusion still reads more confident than the limitations. | Lines 219, 224-226 |
| DA-m2 | Reproducibility | Random seed, optimizer details beyond Adam, batch size, validation protocol, and normalization are missing. | Lines 137, 146-158 |

## Consensus analysis

### Consensus-4

All non-DA reviewers agree that the paper has a credible and useful core but needs major revision before submission. All agree that the biggest issue is not writing quality but validation transparency.

### Consensus-3

EIC, Methodology, and Perspective reviewers agree that the PC int8 vs ESP32-S3 relationship must be explained before the deployment claim is credible. The Domain reviewer treats this as less central to acoustic-domain contribution, but still agrees that the current results need clearer protocol.

### Disagreements

**Is the "pipeline" framing fatal?**  
EIC and Perspective reviewers see it as overclaiming but fixable. Methodology reviewer does not treat it as central if the methods are clarified. Editor resolution: keep the two-stage framing, but change the title and abstract to emphasize post-trigger classifier validation.

**Is a new experiment mandatory?**  
Methodology reviewer strongly prefers repeated/grouped validation. EIC would accept a single-split paper only if framed as preliminary and fully transparent. Editor resolution: at minimum add confidence intervals and confusion matrices; if possible, add repeated or grouped validation. For a stronger venue, grouped validation should be treated as mandatory.

## Revision roadmap

### Priority 1 - Must fix before submission

1. **Audit PC int8 vs ESP32-S3 equivalence.**
   - Acceptance criteria: manuscript states whether both modes used identical model, tensors, labels, scales, zero points, threshold, and post-processing; adds output-difference check or explains why results are not expected to match.

2. **Add evaluation denominator table.**
   - Acceptance criteria: every result row has `N`, positive count, negative count, TP, FP, TN, FN, and whether instances are clean events, corrupted variants, repeated perturbation draws, or averages.

3. **Define thresholding and model selection.**
   - Acceptance criteria: paper states threshold, validation set or threshold rule, checkpoint/model selection protocol, and whether any parameter was tuned on test data.

4. **Add uncertainty reporting.**
   - Acceptance criteria: confidence intervals or bootstrap intervals for MCC/precision/recall/F1; if not possible, explicitly downgrade claims to pilot evidence.

5. **Define MFCC-domain SNR perturbation mathematically.**
   - Acceptance criteria: formula for noise variance, power convention, normalization order, and whether statistics are train-only.

### Priority 2 - Should fix to strengthen the paper

6. **Add grouped/session metadata or limitation.**
   - Acceptance criteria: dataset table includes session/scenario variables if available; otherwise limitation states that event-level split may be optimistic.

7. **Add baseline or explain omission.**
   - Acceptance criteria: include simple MFCC baseline or state why the paper is deployment validation rather than architecture benchmarking.

8. **Expand embedded resource reporting.**
   - Acceptance criteria: model size, RAM/flash, operator set, CPU frequency, latency measurement protocol, and whether feature extraction is excluded.

9. **Clarify impact-event and negative-class composition.**
   - Acceptance criteria: non-launch class taxonomy and whether impacts are included, excluded, or future work.

10. **Add system boundary figure/table.**
   - Acceptance criteria: clearly separates `validated in this paper`, `previously validated`, and `future work`.

### Priority 3 - Text and presentation

11. Retitle toward the measured scope, for example: `Embedded Validation of a Lightweight Post-Trigger MFCC-CNN for Artillery Launch Classification`.
12. Shorten repeated caveats by consolidating them into one crisp scope paragraph plus one limitations paragraph.
13. Add absolute counts to the tables or move extended metrics to an appendix if page-limited.
14. Check whether all bibliography entries cited in the `.tex` are used and whether unused entries should remain.

## Suggested revised claim

Current claim is close, but I would make it more conservative:

> We evaluate a lightweight MFCC-CNN as the post-trigger classification stage of an artillery-launch acoustic sensing pipeline. On a 706-event corpus, we compare float32, PC-side int8, and ESP32-S3 int8 inference under controlled MFCC-domain perturbations. The results indicate that quantized classifier inference can remain practical on ESP32-S3 under the tested feature-space conditions, while full system-level robustness, waveform-domain noise tolerance, and continuous-trigger false-alarm behavior remain outside the present evaluation.

## Minimum submission checklist

- [ ] Table with train/test class counts before and after augmentation.
- [ ] Table with per-row TP/FP/TN/FN and denominator.
- [ ] Threshold and model-selection paragraph.
- [ ] Quantization and representative-dataset paragraph.
- [ ] PC int8 vs ESP32-S3 equivalence check.
- [ ] Confidence intervals or repeated/grouped validation.
- [ ] Dataset context table.
- [ ] Expanded embedded resource table.
- [ ] Title/abstract adjusted to post-trigger classifier scope.


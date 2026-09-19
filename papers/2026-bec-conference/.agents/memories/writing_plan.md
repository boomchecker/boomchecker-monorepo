# Writing Plan Memory

## Drafting Strategy
- `confirmed`: Use `article/article_main.tex` as scaffold; refine content to match locked scope and dual claim.
- `confirmed`: Keep structure compact for 4-6 pages including references.
- `confirmed`: Make the narrative explicitly two-stage, but keep the experimental novelty on the post-trigger classifier only.
- `confirmed`: Present the core contributions in a compact prose paragraph rather than as an explicit numbered list.

## Section-Level Build Order
1. `confirmed`: Introduction -> problem relevance, two-stage framing, implicit objective/claim framing, and 3-point contribution list.
2. `confirmed`: Related Work -> position against prior acoustic detection/classification and embedded deployment studies.
3. `confirmed`: Method -> concise two-stage pipeline framing, 706-event corpus, nominal MFCC-space SNR protocol, compact CNN, and TFLM classifier deployment.
4. `confirmed`: Experimental Setup -> only thesis-backed setup essentials with split-before-augmentation and anti-leakage wording.
5. `confirmed`: Results -> PC-float32 robustness first, then PC-side int8 verification, then ESP32-S3 deployment consistency.
6. `confirmed`: Discussion/Limitations -> MFCC-space robustness boundaries, classifier-only embedded validation boundaries, dataset scale, and external validity bounds.
7. `confirmed`: Conclusion -> practical embedded detection takeaway and bounded future work.

## Figure/Table Plan
- `confirmed`: Table A: PC-float32 robustness metrics across nominal SNR levels.
- `confirmed`: Table B: compact quantized comparison (`PC-int8` vs `ESP32-S3`) across the same nominal SNR levels.
- `confirmed`: Historical 152-event Phase A configuration is mentioned only briefly as development-stage motivation, not as a headline result table.

## Editing Guardrails
- `confirmed`: Prefer high signal density; remove non-essential thesis detail.
- `confirmed`: Keep all central claims explicitly traceable to thesis evidence.
- `confirmed`: Maintain citation-to-argument mapping from `literature_map.md`.
- `confirmed`: Avoid claiming isolated augmentation effects when dataset expansion, imbalance mitigation, and quantization changed jointly.
- `confirmed`: Keep research objective and claims implicit in prose; avoid explicit `RQ` / `H1` labeling in the embedded-paper style.
- `confirmed`: Define the perturbation metric once as `nominal MFCC-space SNR` and then refer to it more compactly as `nominal SNR` in prose and `SNR (dB)` in tables.

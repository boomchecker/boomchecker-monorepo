# Vision & Scope Memory

## Objective
- `confirmed`: Prepare a 4-6 page English IEEE-style conference paper for BEC 2026 on acoustic artillery event detection for real deployment constraints.

## Primary Claims
- `confirmed`: Binary artillery launch detection is framed as a two-stage acoustic pipeline, with this article focusing experimentally on the post-trigger classification stage.
- `confirmed`: A robustness-oriented training strategy can preserve post-trigger artillery launch classification under controlled time-domain noise degradation.
- `confirmed`: The lightweight post-trigger classifier can retain practical behavior on ESP32-S3 with TensorFlow Lite Micro inference when compared against PC-side int8 inference.
- `confirmed`: The practical value is joint: robust post-trigger classification + efficiency-oriented embedded deployment.

## Audience
- `confirmed`: Embedded/CPS and intelligent signal processing reviewers (BEC 2026 tracks).

## Locked Scope
- `confirmed`: Task is binary event detection only (artillery vs non-artillery).
- `confirmed`: Results and figures are reused from the fixed thesis experiment base (`ctuthesis_master.pdf`).
- `confirmed`: No new experiments are planned for this article.
- `confirmed`: Main paper story is based on the expanded Phase B corpus only.
- `confirmed`: Main robustness framing uses nominal MFCC-space SNR / controlled MFCC-space perturbation, not waveform-level time-domain corruption.
- `confirmed`: Terminology lock: the first explicit definition should use `nominal MFCC-space SNR`; subsequent mentions may use `nominal SNR` in prose and `SNR` in compact result/table contexts.
- `confirmed`: The main clean corpus is 706 labeled events (62 artillery launch, 644 non-launch).
- `confirmed`: Data acquisition and median-filter trigger generation are outside the scope of this article; they are treated as previously validated system components.

## Out of Scope
- `confirmed`: Localization algorithm development.
- `confirmed`: Multi-class weapon/variant identification as a main objective.
- `confirmed`: New data collection campaign.
- `confirmed`: Full end-to-end trigger-performance evaluation in continuous audio streams within this article.

## Current Draft Alignment (`article/article_main.tex`)
- `confirmed`: Draft follows intended section flow: Introduction, State of the Art, Method, Experimental Setup, Results, Discussion, Conclusion.
- `confirmed`: Results should be structured as PC-float32 robustness, PC-side int8 verification, and ESP32-S3 deployment results.
- `to-verify`: Every numeric result/table value in draft must be cross-checked against thesis final tables before submission.

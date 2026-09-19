# Evidence Reuse Plan Memory

## Status
- `confirmed`: No new experiments will be run for this article.
- `confirmed`: This file tracks evidence selection and transfer from thesis outputs.

## Evidence Blocks to Reuse
- `confirmed`: Robustness-vs-SNR comparison on the main 706-event corpus under controlled MFCC-space perturbation with nominal MFCC-space SNR levels.
- `confirmed`: PC float32 vs PC int8 verification comparison.
- `confirmed`: PC int8 vs ESP32-S3 TensorFlow Lite Micro deployment comparison.
- `confirmed`: Expanded non-launch corpus and class-imbalance mitigation are part of the final robustness strategy, not a separate ablation headline.

## Transfer Requirements
- `to-verify`: Each transferred numeric claim must match thesis final result tables/plots.
- `to-verify`: Any value currently in `article/article_main.tex` must be validated before final freeze.
- `confirmed`: If a thesis figure is too dense, create a compact article table/summary while preserving exact values.
- `todo`: Confirm and report deployment metrics for the article: inference latency, model size, memory footprint, and quantization scheme for the ESP32-S3 deployment.
- `confirmed`: Reported ESP32-S3 deployment figures currently locked from thesis text: average inference latency `~32 ms` and `tensor_arena` working memory `80 KB`.
- `confirmed`: Main article tables should be split into two smaller tables rather than one all-mode comparison table.

## Exclusion Rules
- `confirmed`: Do not add speculative measurements that are not in thesis evidence.
- `confirmed`: Do not expand into localization experiments.
- `confirmed`: Do not introduce new dataset splits that were not part of thesis results.
- `confirmed`: Do not present the 152-event development-stage configuration as a headline baseline-vs-augmented comparison in the main paper.

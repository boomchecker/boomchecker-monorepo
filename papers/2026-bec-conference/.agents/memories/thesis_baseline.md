# Thesis Baseline Memory

## Source of Truth
- `confirmed`: Master thesis PDF (`ctuthesis_master.pdf`) is the canonical empirical baseline.
- `confirmed`: Thesis experiment outcomes are fixed; article will selectively reuse, compress, or reframe.

## Baseline Pipeline (as represented in current article draft)
- `confirmed`: Impulse-centered segmentation around peak events.
- `confirmed`: MFCC feature extraction.
- `confirmed`: Compact CNN classifier.
- `confirmed`: Export/inference via TensorFlow Lite on embedded target.
- `confirmed`: Article-level system framing is two-stage: prior acquisition/trigger component plus post-trigger classifier evaluated here.

## Baseline Dataset/Setup Notes (from current draft to verify against thesis)
- `confirmed`: Main article corpus should be defined as 706 clean labeled events (62 artillery launch / 644 non-launch).
- `confirmed`: The earlier 152-event configuration is development-stage context only and should not anchor the main paper evaluation.
- `to-verify`: MFCC input tensor currently stated as (58, 12, 1).
- `confirmed`: MFCC extraction details now locked for article drafting: `fs = 22.05 kHz`, `60 ms` segment, frame length `2.5 ms` (`~55` samples), hop `1.0 ms` (`~22` samples), overlap `1.5 ms` (`60%`), Hamming window, `NFFT = 512`, `40` mel filters, first `12` MFCC coefficients retained, resulting tensor `(58, 12, 1)`.
- `to-verify`: CNN topology details in draft (Conv32/Conv64 + Dense64 + Dropout0.5).
- `confirmed`: Final training settings available for article reuse: `Adam` optimizer, `Binary Crossentropy` loss, `50` epochs, and `Dropout(0.5)` in the final model.
- `confirmed`: Class-imbalance handling can be described safely at the principle level: the 706-event corpus contains only about `8.8%` positive artillery-launch samples, so class-weighted loss was used to penalize minority-class errors more strongly.
- `confirmed`: Main robustness evaluation should be described as controlled MFCC-space perturbation using nominal MFCC-space SNR levels, not waveform-level time-domain corruption.
- `confirmed`: Main split logic is event-level hold-out split before augmentation.
- `confirmed`: Embedded implementation details available for article reuse include `tensor_arena = 80 KB`, selective `MicroMutableOpResolver` operator loading, and average inference latency `~32 ms`.
- `confirmed`: Safe acquisition-chain details for article reuse: `PreSonus PRM1` microphones, `Roland Rubix 44` audio interface, and 16-bit signal representation in the article-level processing description; omit unrelated playback hardware.

## Reuse Policy From Thesis to Article
- `confirmed`: Reuse key detection and robustness results.
- `confirmed`: Reuse PC vs embedded comparison outcomes.
- `confirmed`: Reuse PC-side int8 verification results as the bridge between float32 desktop inference and ESP32-S3 deployment.
- `confirmed`: Reuse selected figures/tables where directly relevant to dual claim.
- `confirmed`: Remove thesis content that does not strengthen binary detection + embedded deployment storyline.
- `confirmed`: Rewrite narrative for conference brevity and claim clarity.

## Figure/Table Asset Intent
- `draft`: Keep one core SNR robustness table and one compact PC-vs-embedded comparison artifact.
- `draft`: Prefer minimal high-signal visuals due to 4-6 page limit.

# Evidence / Experiment Plan Memory

## Status
- `confirmed`: No experiments have been run for this paper yet; this file tracks what exists already vs. what needs to be produced.

## Existing Assets to Reuse or Extend
- `confirmed`: `scripts/lms-filter/csrc/` — fixed-point (Q15) FxLMS C core (SISO, sum-first, MISO configurations), with C unit tests.
- `confirmed`: `scripts/lms-filter/python/lms_demo.py` — Python harness building a synthetic drone-reference + primary-path + optional wanted-signal scenario, producing WAVs/plots/`metrics.json` (attenuation in dB, tail MSE).
- `confirmed`: `scripts/lms-filter/drone-spectrum/` — DADS dataset spectral analysis (average PSD for drone vs. non-drone recordings), already used to justify BPF-band focus for ANC.
- `confirmed`: `scripts/lms-filter/report/main.tex` — earlier Czech-language internal report describing the FxLMS ANC demo (MISO 4x4 achieved 21.3 dB attenuation vs. 6.7 dB for sum-first vs. 24.5 dB for single-motor SISO in that internal report — needs re-verification, not yet English/ICASSP-ready, and uses a synthetic/simulated acoustic path only).
- `confirmed`: `projects/2026-zelinjak-artillery-detection/BEC/` — impulsive artillery-event classifier (MFCC + compact CNN), robustness-to-noise evaluation, and ESP32-S3 TFLite Micro deployment; candidate downstream detector for a "noise suppression improves detection" story.

## Gaps to Close for ICASSP-Level Rigor
- `to-verify`: Current FxLMS results use a simulated/synthetic secondary acoustic path, not measured on real hardware — decide if that's acceptable or if real-path measurement is needed.
- `to-verify`: No end-to-end evaluation yet connecting FxLMS-suppressed audio to actual impulsive-event detection metrics (accuracy/F1 before vs. after suppression) — this is likely the key new experiment for this paper.
- `to-verify`: Need a clear noise-suppression evaluation protocol (attenuation vs. SNR sweep, multiple noise references/actuator counts) with reproducible metrics for a paper table.
- `to-verify`: Decide on embedded/real-time feasibility claims (would reuse the fixed-point C core and possibly report cycle counts/latency on a target MCU).

## Transfer Requirements
- `to-verify`: Any numeric claim carried over from the internal Czech report (`scripts/lms-filter/report/main.tex`) must be re-verified/re-run before being cited in the English ICASSP paper.

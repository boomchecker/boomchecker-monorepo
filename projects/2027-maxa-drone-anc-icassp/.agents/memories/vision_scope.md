# Vision & Scope Memory

## Objective
- `confirmed`: Prepare an English IEEE-style conference paper for the 2027 IEEE International Conference on Acoustics, Speech, and Signal Processing (ICASSP 2027) on suppressing drone (multirotor UAV) ego-noise to improve detection of impulsive acoustic events.
- `to-verify`: Exact ICASSP 2027 paper length/format limits, submission deadlines, and track name once the CFP is published — do not assume BEC-style limits carry over.

## Primary Claims (draft, to refine)
- `to-verify`: An adaptive/active noise suppression stage (FxLMS-based, driven by motor-derived or synthetic references) can substantially attenuate on-board drone ego-noise without requiring a dedicated reference microphone.
- `to-verify`: Suppressing ego-noise ahead of an impulsive-event detector/classifier improves detection performance (or preserves it under harsher noise) compared to the unprocessed microphone signal.
- `to-verify`: The approach is compatible with lightweight/embedded real-time implementation (fixed-point, low-latency).

## Relationship to Existing Work
- `confirmed`: Builds on the FxLMS ANC prototype and DADS-based drone-spectrum analysis already implemented in `scripts/lms-filter/` (C core + Python harness + prior Czech-language internal report in `scripts/lms-filter/report/`).
- `confirmed`: Builds on the impulsive acoustic event (artillery launch) detection pipeline documented in `projects/2026-zelinjak-artillery-detection/BEC/` (thesis + BEC2026 paper), which this new paper can reuse as the "downstream detector" half of the story.
- `to-verify`: Decide whether this paper reuses the BEC2026 artillery-detector as the downstream task, or targets a more generic/public impulsive-event benchmark for a broader ICASSP audience.

## Audience
- `confirmed`: ICASSP audio/acoustic signal processing reviewers — expect rigor on adaptive filtering theory, quantitative attenuation metrics, and a clearly separated evaluation of the downstream detection task.

## Out of Scope (draft, to refine with user)
- `to-verify`: New hardware data collection campaign vs. reuse of DADS + simulated mixtures + existing artillery/impulse corpus.
- `to-verify`: Whether embedded (ESP32/STM32) real-time deployment results are required for this paper or left as future work.

## Working Assumption
- `confirmed`: This folder currently holds only the paper-writing scaffold (IEEE template + memories). No article content has been drafted yet beyond a section skeleton.

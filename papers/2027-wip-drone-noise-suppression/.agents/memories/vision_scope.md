# Vision & Scope Memory

## Objective
- `confirmed`: Prepare an English IEEE-style conference paper on suppressing drone (multirotor UAV) ego-noise to improve detection of impulsive acoustic events.
- `confirmed`: No target venue yet; it is chosen once the measurement campaign is finished and the results are known.
- `to-verify`: Paper length/format limits, submission deadline, and track name of whichever venue is chosen; do not assume BEC-style limits carry over.

## Primary Claims (draft, to refine)
- `to-verify`: An adaptive/active noise suppression stage (FxLMS-based, driven by motor-derived or synthetic references) can substantially attenuate on-board drone ego-noise without requiring a dedicated reference microphone.
- `to-verify`: Suppressing ego-noise ahead of an impulsive-event detector/classifier improves detection performance (or preserves it under harsher noise) compared to the unprocessed microphone signal.
- `to-verify`: The approach is compatible with lightweight/embedded real-time implementation (fixed-point, low-latency).

## Relationship to Existing Work
- `confirmed`: Builds on the FxLMS ANC prototype and DADS-based drone-spectrum analysis already implemented in `projects/2026-maxam-fxlms-filter/` (C core + Python harness + prior Czech-language internal report in `projects/2026-maxam-fxlms-filter/report/`).
- `confirmed`: Builds on the impulsive acoustic event (artillery launch) detection pipeline documented in `papers/2026-bec-conference/` (BEC2026 paper) over `projects/2026-zelinjak-artillery-detection/` (dataset + ml pipeline), which this new paper can reuse as the "downstream detector" half of the story.
- `to-verify`: Decide whether this paper reuses the BEC2026 artillery-detector as the downstream task, or targets a more generic/public impulsive-event benchmark for a broader signal-processing audience.

## Audience
- `confirmed`: Audio/acoustic signal processing reviewers at an IEEE-style venue; expect rigor on adaptive filtering theory, quantitative attenuation metrics, and a clearly separated evaluation of the downstream detection task.

## Out of Scope (draft, to refine with user)
- `confirmed`: A new hardware measurement campaign is in scope and is the blocking item for the whole paper (it is what the venue choice waits on); DADS + simulated mixtures + the existing artillery/impulse corpus stay as the complement, not the replacement.
- `to-verify`: Whether embedded (ESP32/STM32) real-time deployment results are required for this paper or left as future work.

## Working Assumption
- `confirmed`: This folder currently holds only the paper-writing scaffold (IEEE template + memories). No article content has been drafted yet beyond a section skeleton.

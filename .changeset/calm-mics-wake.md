---
"fw-bom-stm32node": patch
"fw-common-boomdetect": patch
---

No more pop at the start of every `stream` and `detect`.

Each run restarts the microphone, and the DC blocker was seeded while the mic
was still powering up. The settled mic then sat on another DC level, the x16
gain clipped the difference to full scale for ~0.12 s and the 43 ms blocker
took ~0.4 s to remove it. Now the blocker tracks fast for the first 85 ms
(output muted) and the board drops the first 107 ms, so a stream starts with
settled audio. What remains is the mic's own sub-20 Hz drift, ~0.005 of full
scale over the first ~0.4 s.

Field recordings made before the fix still carry the pop: the training package
now cuts 0.5 s (was 0.2 s, which left the tail) from a clip that starts at a
stream's first sample.

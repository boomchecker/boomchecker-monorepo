---
"fw-bom-stm32node": patch
---

The microphone's PCM keeps its low bits.

The x16 (+24 dB) gain of the PDM -> PCM chain was applied after the FIR's
`>> 15`, so it multiplied an already rounded value: every sample was a
multiple of 16, a 12-bit stream whose rounding noise (about -77 dBFS) sat near
the microphone's own noise floor and, in quiet outdoor recordings, only 8-11 dB
under the 4-8 kHz background. The gain now goes in before the FIR, where
nothing has been rounded yet. Scale, clipping level and the USB format are
unchanged; in a host simulation of the chain the noise floor of a silent mic
drops by 7-15 dB below 4 kHz, and a -56 dBFS tone comes out at the same level.

Models need no retraining: their features do not depend on the level, and the
squelch compares the same RMS scale.

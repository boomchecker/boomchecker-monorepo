---
"fw-bom-stm32node": patch
---

The PDM -> PCM conversion takes about a quarter of the time it used to.

The CIC decimator ran its five integrators and combs as 64-bit values kept in
the DSP state struct and walked all 16 bits of every halfword against the slot
mask. It now keeps them in 32-bit registers for the whole ring half and steps
through the selected microphone's 8 bits, listed once at init. A CIC's output
stays exact when its registers wrap, as long as they cover the output range:
5 x log2(64) bits of growth plus the sign = 31 bits. The 64-bit integrators
wrapped too (a microphone's DC offset overflows them within tens of milliseconds), so
the samples are bit for bit the same, on both slots, quiet or clipping.

Measured on the board, one 21.33 ms ring half: CIC 14.8 -> 3.0 ms, the whole
conversion 17.9 -> 6.2 ms at the core's 240 MHz; `detect` reports `h=` of
about 5.9 ms instead of 17.2 ms; everything after the CIC takes as long as
before. pdm_pcm.h refuses to build if a larger decimation or order would no
longer fit in 32 bits, or if a slot mask stops selecting 8 bits.

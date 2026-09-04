---
"fw-bom-stm32node": minor
---

On-device acoustic drone detection on bom-stm32node, plus the GNSS and
bring-up tooling that came with it.

`detect <sec> [squelch_milli] [thr_milli] [dbg]` consumes microphone PCM,
decimates 48 -> 16 kHz by phase-carrying /3 pick (the pdm_pcm 8 kHz FIR is the
anti-alias), extracts MFCC frames with CMSIS-DSP (1024/512), aggregates every
run of 14 frames above the RMS squelch into a 52-value feature vector
([mean, std, dmean, cmax] x 13) and classifies it with the model compiled into
the firmware - currently a 51->32->1 MLP whose decision value is a raw logit,
operating point +7.25. Results stream as `LVL`/`DET`/`DETEND` console lines.
Vendors CMSIS-DSP v1.15.0 under `third_party/` (Apache-2.0, one local patch to
`arm_mfcc_f32.c` recorded in `third_party/CMSIS-DSP/patches/`).

Also: a Teseo-LIV3R GNSS console (`gps`, `gpstx`, `gpsrst`) over UART4 with
per-flag UART error counters; `micdiag`, which probes the PDM data pins as GPIO
to tell a silent microphone from a mis-wired one; and `dfu`, which jumps to the
STM32H5 ROM bootloader so the board reflashes over the same USB port without an
ST-Link. The main stack grew to 16 KB after `detect` was found to overflow
MSPLIM once a USB interrupt nested on top of snprintf.

The PDM slot mask - which microphone of a pair the DSP demodulates - is now a
runtime setting (`micslot [a|b]`) with a documented default rather than a
hard-coded constant. It was previously fixed to A, which on a board populated
the other way decodes the empty half of every frame and reports perfect silence.

The default `detect` threshold moves from 7250 to 15000. The 7.25 operating
point was selected offline against recordings; measured on hardware with a
working microphone it fires on ordinary room noise (23 of 396 windows over
three minutes, peaking at 12.05). 15.0 gave zero alarms over a second
three-minute campaign. This was measured against ambient noise with no drone
present, so the sensitivity it gives up is unquantified.

Correctness fixes on the way in: the model header is now static-asserted
against the feature layout it is indexed with, the detector FIFO refuses to
overwrite unread samples, `detect`/`gps`/`micdiag` always emit their trailer
line so a host never waits on a run that already failed, and `micdiag` derives
pin positions from the CubeMX pin macros instead of hard-coded bit numbers.

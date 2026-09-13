# Acoustic-Based Detection, Classification, and Localization of Impulsive Sounds

**Authors:** Jakub Svatoš, Martin Maxa (CTU FEE, Department of Measurement)
**Corresponding author on the abstract:** Dr. Jakub Svatoš, svatoja1@fel.cvut.cz
**Presented by:** Martin Maxa — **Wednesday 2026-09-16**
**Submission ID:** 28

> **TODO:** fill in the exact conference name and edition. The abstract uses an IMEKO
> template (the PDF metadata carries "XVII IMEKO World Congress" from the template
> author D. Ilić), which identifies the template's origin, not a confirmed venue.
> Rename this directory accordingly once known.

## Contents

```
abstract/   submitted abstract (PDF, created 2026-09-08)
slides/     presentation (beamer) — slides.tex, compiled slides.pdf, figs/
```

## Build

```bash
task build      # from this directory; or task papers:imeko26:build from the root
task clean
```

Needs `pdflatex` and beamer — both are in the `sw-devcontainer`; on a bare system run
`task latex:setup` from the repository root.

## What it is about

A distributed network of stand-alone acoustic sensors plus a remote server. Each sensor
continuously monitors its surroundings and transmits detected signals to the server for
processing. The system classifies acoustic events and determines their 2D position by
triangulating across multiple sensors. Tested with various firearm calibers and
ammunition.

The core of the contribution is a **comparison of cepstral feature extractors** (MFCC,
LFCC, IMFCC, GTCC) paired with NN and SVM classifiers, across three frame lengths
(15 / 30 / 50 ms).

Conclusions from the abstract:

- MFCC, LFCC and IMFCC yield comparable results; **GTCC is consistently better**.
- **NN outperforms SVM**, thanks to its ability to model complex patterns.
- Longer frames improve accuracy but begin to capture environmental reflections.

## Provenance of the numbers

Table I in `slides/slides.tex` is transcribed **verbatim from the submitted abstract**
(`abstract/28_Maxa_*.pdf`), decimal commas included. It reports results for the 9 mm
class. If the presentation is meant to show different numbers, the original evaluation
outputs need to be located — they are not in this repository yet.

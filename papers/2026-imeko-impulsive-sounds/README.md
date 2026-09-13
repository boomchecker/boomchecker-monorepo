# Acoustic-Based Detection, Classification, and Localization of Impulsive Sounds

**Authors:** Jakub Svatoš, Martin Maxa (CTU FEE, Department of Measurement)
**Corresponding author on the abstract:** Dr. Jakub Svatoš, svatoja1@fel.cvut.cz
**Presented by:** Martin Maxa — **Wednesday 2026-09-16**
**Submission ID:** 28
**Slot:** 10 minutes, Q&A separate. The script is written to 9:30.

> **TODO — the one thing still unknown:** the exact conference name and edition.
> The abstract uses an IMEKO template (the PDF metadata carries "XVII IMEKO World
> Congress" from the template author D. Ilić), which identifies the template's origin,
> not a confirmed venue. Until it is confirmed, the title slide shows a red
> `⟨CONFERENCE NAME — TO BE CONFIRMED⟩` placeholder — see `\date` in `slides/slides.tex`.
> Rename this directory once it is known.

## Contents

```
abstract/   submitted abstract (PDF, created 2026-09-08)
slides/     slides.tex + the training material built from it
```

Inside `slides/`:

| File | What it is |
|---|---|
| `slides.tex` | the deck **and** the spoken script (in `\note{}` blocks) |
| `slides.pdf` | the deck as presented — 10 frames + 4 appendix |
| `slides-notes.tex` → `slides-notes.pdf` | 10 pages, script + slide thumbnail — **print this and learn from it** |
| `cue-card.tex` → `cue-card.pdf` | one dense A4 for the lectern: every slide's opening line verbatim, plus beats and numbers |
| `QA.md` | expected questions, answers, and which appendix slide to jump to |
| `figs/` | `gunshot_char.png`, `tdoa.png` |

## Build

```bash
task all        # deck + notes + cue card
task build      # deck only
task notes      # speaker-notes PDF
task cue        # lectern cue card
task clean
```

Needs `pdflatex`, `beamer` and `pgfpages` — all in the `sw-devcontainer`; on a bare
system run `task latex:setup` from the repository root.

## The script lives in one place

The spoken script is in the `\note{}` blocks of `slides.tex`, and nowhere else.
`slides-notes.tex` is a two-line driver that sets `\SHOWNOTES` and `\input`s
`slides.tex`, so the notes build cannot drift from the slides. **Do not create a
separate script file** — editing the wording in two places is how you end up
rehearsing a version that is no longer on screen.

`slides-notes.pdf` is built with `show only notes`, so it is 10 pages — one per
content slide, each carrying a thumbnail of its slide in the top right corner. It does
not repeat the full-size slides; for those, print `slides.pdf`.

Conventions inside the notes:

- `//` — pause. There are 57 of them, and they are worth about 40 s of the running
  time, so they are not decoration.
- `[CLICK]` — advance the overlay. Two in the deck, on the signature and localization
  slides.
- `[CUT IF LATE]` — a sentence that can be dropped live without breaking the argument
  or the enumeration around it. Three of them, ≈35 s in total. The cue card names them.
- Each note opens with its time window.

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

Table I in `slides/slides.tex` reports the 9 mm class and is transcribed from the
submitted abstract (`abstract/28_Maxa_*.pdf`). The **values are verbatim**; only the
decimal separator was normalized from commas to points, because the talk is in English
and the rest of the deck uses points.

The abstract does not say what those percentages are computed over. That comes from the
study the abstract summarizes:

> Svatoš, J. & Holub, J., *Cepstral coefficients effectiveness for gunshot classifying*,
> Meas. Sci. Technol. **35** (2024) 076122.

From its Table 2, the labeled corpus is five classes, 371 training and 118 test samples:

| Class | | Train | Test |
|---|---|---:|---:|
| 0 | False alarms | 86 | 21 |
| 1 | 9 mm | 70 | 31 |
| 2 | 5.56 NATO SD | 100 | 31 |
| 3 | 7.62 mm Tokarev | 55 | 17 |
| 4 | .22 | 60 | 18 |

which makes the headline cell check out exactly: ACC 99.15 % is 117 of 118, and
RLC 96.77 % is 30 of 31. Also from that paper: 26 features per extractor, an
approximately 75/25 split, and an NN trained in MATLAB with Levenberg–Marquardt,
converged after 370 iterations. All of this is on appendix slide 11.

**RLC is Recall** (also called sensitivity), per eqs. (1)–(3) of the same paper — this
was previously an unverified guess in the deck.

## Localization figures

`figs/tdoa.png` and the bearing equation come from the earlier talk *Spectral Method
for Acoustic Impulse Events TDoA Estimation* (Maxa & Svatoš). The accuracy quoted on
the slide is computed from `scripts/tdoa_estimation/data_*.jsonl` in this repository —
70 events at nine reference angles from −45° to +60°, with `d = 0.186 m` and
`f_s = 44.1 kHz` taken from `analyzer_live.py`:

| Method | MAE | Median | Max |
|---|---:|---:|---:|
| Cross-correlation | 0.84° | 0.41° | 7.61° |
| Cross-correlation + parabolic interpolation | **0.77°** | 0.56° | 6.67° |
| PBDE | 3.24° | 1.80° | 29.0° |
| PBDE + linear regression | 3.91° | 3.55° | 12.15° |

Note this is **bearing** accuracy for a single unit, not 2D position error — the talk
and `QA.md` are careful to say so.

## Reference literature

Several source PDFs used while writing the talk sit in `slides/` and are
**git-ignored** (13 MB, and not ours to redistribute): the Svatoš & Holub 2024 paper,
the RTSI 2024 smart-cities paper, Jabali's 2023 diploma thesis, and the earlier TDoA
presentation. Only the extracted figures under `figs/` are tracked.

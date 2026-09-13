# Q&A preparation

Answer, then the backup slide to jump to. Backup slides are frames 11–14 of
`slides.pdf` (experimental setup, bearing estimation, other cepstral coefficients,
metrics).

Every number below is sourced. Where it is not, the entry says so — do not improvise
a figure on stage.

---

## The one you will almost certainly get

**"99 % accuracy — on how much data? Isn't that overfitted?"**

Answer it with the numbers, then concede the limitation yourself:

> The test partition has 118 events across five classes, 31 of them 9 mm. So 99.15 %
> is 117 of 118 correct, and the 96.77 % recall is 30 of the 31 nine-millimetre shots.
> Training used 371 samples, roughly a 75/25 split, with 26 features per extractor.
> It is a small corpus — that is the honest limitation of the study, and it is why we
> report Matthews correlation alongside accuracy rather than accuracy alone.

→ **backup slide 11.** Source: Svatoš & Holub 2024, Table 2.

Conceding the dataset size is stronger than defending it. The claim the paper makes is
*relative* — GTCC beats the mel family — and that comparison holds on a small corpus
because every method saw the same data.

---

## Method

**"Why does GTCC win?"**
Lead with what is sourced, and label the rest as your reading:

> The gammatone filter bank was designed to model the response of the human cochlea,
> and in general audio work it is known to be more robust to noise than MFCC — that is
> what the abstract says, and it carries over here. We did not isolate the mechanism in
> this study. My own reading is that the gammatone bank keeps finer detail at low
> frequencies, where the muzzle blast has most of its energy — but that is a
> hypothesis, not a result.

Do **not** state the low-frequency explanation as fact. It is plausible, it is not in
the abstract or the source paper, and Svatoš may be in the room.
→ **backup slide 13.**

**"In your table the SVM beats the NN for LFCC at 30 and 50 ms. So does NN really win?"**
It does not win every row, and say so:

> Correct — for LFCC the SVM is ahead at 30 and 50 ms, by about three percentage
> points. The statement in the abstract is about the study as a whole: across all
> extractors and classes the network comes out slightly ahead, and more so at longer
> frames. On this one-class table it is a tendency, not a rule.

The abstract's exact wording is "NN achieves slightly better classification success,
especially with longer frames". Stay inside that.

**"Why not a deep network / a CNN on spectrograms?"**
With 26 features and a few hundred samples, a shallow network is the right capacity.
The NN here is a MATLAB Neural Fitting network trained with Levenberg–Marquardt,
converged in 370 iterations. A CNN on this corpus would mostly memorize it.
*(Follow-on work does use a CNN — see the embedded question below.)*

**"Why do longer frames stop helping?"**
A 50 ms frame starts to contain the acoustic impulse response of the place the sensor
stands in — reflections, reverberation. Past some length you are classifying the room
rather than the gunshot. You can see that tail in the measured waveform on slide 3.

**"What is RLC?"**
Recall, also called sensitivity — the share of actual gunshots of that class that were
detected. → **backup slide 14.** Source: Svatoš & Holub 2024, eqs. (1)–(3).

---

## Localization

**"How accurate is the localization?"**
Careful — what was measured is the **bearing**, not the final 2D fix. Say that:

> We measured the bearing accuracy of a single unit. Over 70 recorded events at nine
> reference angles from −45° to +60°, cross-correlation with parabolic interpolation
> gives a mean absolute error of 0.77°. The 2D position error depends on the baseline
> between units and the geometry, and we do not have a characterized figure for it.

→ **backup slide 12.**

**"Why cross-correlation rather than the phase-based method?"**
We tried both. Cross-correlation with parabolic interpolation: MAE 0.77°. Phase-based
delay estimation: 3.24°, and adding linear regression made it worse, 3.91°. The phase
method was attractive for noisy conditions but did not pay off here.
→ **backup slide 12.**

**"How many units do you need?"**
With two microphones per unit, two units are enough for a 2D fix, because each unit
already gives a bearing. With one microphone per unit you would need three units for
time-of-arrival, or four for TDoA. → **backup slide 12.**

**"What limits the bearing resolution?"**
Microphone spacing of 0.186 m at 44.1 kHz gives a maximum delay of about 24 samples
end-to-end, so sub-sample interpolation matters — that is exactly the gap between
plain cross-correlation (0.84°) and the parabolic variant (0.77°).

---

## System and deployment

**"How does the detection itself work? You only showed classification."**
Fair — the talk is about the second stage. The trigger is prior work:

> Detection is a streaming trigger on each unit: a median-filter impulse detector that
> estimates the local acoustic background and flags candidate impulsive events. It was
> validated separately — it detected all tested small-arms gunshots in shooting-range
> measurements and operated down to an acoustic SNR of about 5 dB. What I showed today
> is what happens to the candidates it forwards.

Source: Svatoš & Holub, *Smart Acoustic Sensor*, IEEE RTSI 2019 (cited as the trigger
stage in the BEC 2026 paper). No backup slide — it is one sentence.

**"Does this run in real time? On embedded hardware?"**
Not in this study — here the sensors forward detected signals to a server. There is
follow-on work on exactly that question: a compact MFCC-CNN post-trigger classifier
deployed on an ESP32-S3 with TensorFlow Lite Micro, about 32 ms of inference per
segment and an 80 KiB tensor arena.
*(That is the BEC 2026 paper, Maxa, Svatoš & Zelinka — your own work, so you can speak
to it freely.)*

**"How were the recordings made?"**
At a military shooting range, sensor at a fixed position with clear line of sight to
every firing position. Four weapons: 9 mm, 5.56 NATO SD, 7.62 mm Tokarev and .22, each
with a single ammunition type, plus a false-alarm class of gunshot-like impulsive
events. → **backup slide 11.**

**"What counts as a false alarm?"**
Impulsive events with a gunshot-like pattern — door impacts, shouts, and similar. They
are class 0, with 86 training and 21 test samples.

**"What about range and weather?"**
Not characterized in this study. Say so plainly. Long-range acoustic localization is
known to be sensitive to atmosphere and terrain, and that is out of scope here.

---

## Two things you must find out before Wednesday

These are on the slides and you cannot currently answer a follow-up about them:

1. **`figs/gunshot_char.png` — which weapon, what distance, whose recording?** The
   slide says "this is a real recording" and the script reads the 45 ms gap off it.
   The gap between shock wave and muzzle blast is set by geometry, so "what range was
   that?" is the obvious follow-up. Ask Svatoš.
2. **The 70 events behind MAE 0.77° — what were they?** `scripts/tdoa_estimation/`
   holds the data (recorded 2026-04-20, angles set by hand) but not what made the
   sound. If they were gunshots, say so. If they were test impulses, say that instead
   — the bearing algorithm does not care, but the audience will.

## If you do not know

Three questions where you genuinely have no data. Do not invent an answer:

- 2D position error in metres.
- Performance below some SNR for *this* classifier (the SNR sweep belongs to the
  follow-on CNN work, not to this study).
- Distances of the firing positions from the sensor in this particular dataset.

> "That is not something we characterized in this study — I would rather not guess.
> Jakub Svatoš ran that measurement campaign, and I can put you in touch."

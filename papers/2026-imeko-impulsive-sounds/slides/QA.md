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
The gammatone filter bank models the response of the human cochlea. Compared with a
mel scale it resolves the low frequencies more finely, and that is where the energy of
a muzzle blast sits. It is also known to be more noise-resistant than MFCC in general
audio work.
→ **backup slide 13.**

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

## If you do not know

Three questions where you genuinely have no data. Do not invent an answer:

- 2D position error in metres.
- Performance below some SNR for *this* classifier (the SNR sweep belongs to the
  follow-on CNN work, not to this study).
- Distances of the firing positions from the sensor in this particular dataset.

> "That is not something we characterized in this study — I would rather not guess.
> Jakub Svatoš ran that measurement campaign, and I can put you in touch."

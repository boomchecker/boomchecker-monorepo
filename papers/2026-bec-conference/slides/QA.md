# Q&A preparation – BEC 2026

First draft, seeded from the four reviews of the paper (see
`.agents/memories/2026-07-21_bec2026_reviews_camera_ready.md`) – the reviewers already
asked the questions the audience is most likely to ask. Appendix slides: A1 full
robustness table, A2 control: noise on the MFCC features, A3 design choices, A4 the
five ESP32 disagreements, A5 metrics.

Every number below is from `article/article_main.tex`. Do not improvise figures.

---

## The one you will almost certainly get

**"Fifty launches – isn't that far too little?"**

> Yes, it is the main limitation. Fifty physical launch events, one gun type. We split
> by event, so the four channels of one shot never cross partitions, and every number
> is averaged over twenty-five runs. The comparison between augmentation strategies is
> relative – all three saw exactly the same data – so it holds even on a small corpus.
> Generalization to other calibres is untested.

→ **A1**. Test partition: 168 samples, 10 of them launches.

---

## Method

**"Why not anomaly detection – one-class SVM, isolation forest, autoencoder?"**
> After the trigger, every candidate is already anomalous – including small-arms
> gunshots. An anomaly detector would flag a rifle shot too. We need the
> artillery / non-artillery decision, so a supervised classifier. One-class methods fit
> the first stage better. → **A3**

**"Why MFCC and not GTCC, when your own group showed GTCC is better?"**
> For gunshot classification, yes. Here we chose MFCC as the standardized front end
> with mature embedded implementations. Trying GTCC on the MCU is a fair next step. → **A3**

**"Why 22.05 kHz and not 44.1?"**
> The launch energy lies well below 11 kHz. Buffers and MFCC compute scale with the
> sampling rate, so 44.1 kHz would cost RAM without adding usable signal. → **A3**

**"The paper compares feature-domain and waveform-domain augmentation – why is that
only in the appendix?"** / **"Why is the MFCC-jitter model so bad?"**
> It was a control, and it answers a narrow question: independent Gaussian jitter on
> the precomputed MFCC matrix does not imitate noise that has passed through the MFCC –
> additive noise changes the log-mel cepstrum nonlinearly and depending on the signal.
> At 5 dB it reaches MCC 0.30, against 0.98 with waveform noise. A feature-domain model
> that follows how noise actually propagates through the MFCC might work – we did not
> test that. → **A2**
> *(TODO: have the jitter σ ready.)*

## Results

**"Why is the float32 model not better than int8?"**
> The differences stay within ±0.02 MCC, without a consistent direction. So int8 is
> performance-neutral for this classifier – we do not claim it improves robustness.

**"Why do five ESP32 inferences differ from the PC?"**
> All five have a PC score of exactly 0.5. The model is very confident, so one LSB of the
> output spans 10.8 logit units, and TFLite and TFLite Micro may round one LSB
> differently. The other 1,171 of 1,176 are bit-exact. → **A4**

**"Is 32 ms the full latency?"**
> No – only the network inference on one MFCC segment. The MFCCs were computed on the
> host. On-device MFCC and end-to-end latency are future work.
> *(TODO: check whether 32 ms was measured with a Release build – the audit in the
> memory notes says the sdkconfig had debug optimization.)*

**"What about power consumption?"**
> Not measured yet – energy per decision is on the future-work list.

**"Real noise, not Gaussian?"**
> Agreed, Gaussian noise is a simplification. It is still closer to the real front end
> than a perturbation of the features, because it goes through the MFCC. Field noise and propagation
> effects are next.

**"Will you publish the dataset?"**
> *(TODO: agree the answer with the co-authors – military measurements, likely
> "available on request".)*

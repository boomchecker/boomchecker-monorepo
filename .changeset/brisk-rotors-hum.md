---
"fw-common-boomdetect": minor
"fw-bom-stm32node": minor
---

Layout 4: the modulation spectrum of the 1-4 kHz envelope, and the first two models on it, gbt_m1 and mlp_m1.

The 2 October ladder left the spectral features at the background by 60 m: a
DJI Phantom 4 hovering 60-90 m overhead has the same RMS as the empty sky. What
survives there is the rhythm of the sound. The rotors' broadband noise is
amplitude-modulated at the blade-pass rate - about 170-185 Hz for the Phantom 4,
with a harmonic at twice it - while traffic, wind, an air-conditioner and people
have no stable rhythm at all. The pipeline now keeps that envelope beside the
frames: the 1-4 kHz band, rectified and low-passed (three biquads,
`src/envelope.c`, coefficients generated from the training package into
`src/envelope_coefs.h`), sampled at 1 kHz into a ring of two seconds that
follows the audio continuously, gated frames included. The new extractor
`stats_spectral_mod` (`src/extractor_mod.c`, layout 4, 79 values) is layout 2
plus ten numbers from a Welch spectrum of that ring: the strongest line in
50-400 Hz and its frequency, the harmonic, per-band maxima, the share of bins
that are lines, the envelope's depth and the share of its power in 100-250 Hz.

- Judged out of fold on the same four folds as `gbt_f3`, at the 5 FA/h point:
  the DJI at 60/80/90 m goes from 38/44/17 % of windows called drone (the
  `mlp_f2` family on layout 2) to 86/85/71 % (`gbt_m1`) and 87/83/72 %
  (`mlp_m1`); the 30 September take at 70 m from 0 to 78/89 %; the last ten
  seconds of the climb to 100 m from 0 to 64/57 %. At 1 FA/h both alarm on all
  seven 2 October heights with none of 27 own negatives and no alarm in 30 min
  of traffic (layout 2: 5/7 and 3/7). Halmstad window AUC of the forest 0.848 ->
  0.886. The Runner 250 at 40 m gains nothing; the modulation is a signature of
  steady rotor speed and smears during manoeuvres.
- `gbt_m1` (200 trees, 78 inputs, threshold 3.211) and `mlp_m1` (78 -> 32 ->
  16 -> 1, threshold 8.400) join the registry behind `mlp_f2`, which stays the
  default until the board has run them: `model gbt_m1` switches.
- A layout-4 window that closes before the ring holds two seconds - after init,
  and after a gap - carries no decision (`boomdetect_event_t::window.warming`);
  the training package has NaN there and never fitted a model on such a window.
  So the first two seconds of a `detect` run print nothing with these models.
- `boomdetect_extract_fn` gained a side-channel argument (`boomdetect_side_t`:
  the envelope ring), `boomdetect_extractor_t` an `env_required` field and a
  `prepare` hook that boomdetect_init() runs (layout 4's FFT instance and
  window table, 6 ms, used to land in the first window of the real-time loop
  and cost a block); `boomdetect_t` grew by the 8 KB ring, which is only fed
  for an extractor that asks for it. The firmware's detect path needs no
  change: the extractor follows the model's layout as before.
- On board B (2026-10-04, selftest MATCH 21): a plain frame costs 1213 us with
  the envelope running (1073 us for a layout-2 model, unchanged), the frame
  that closes a window 4.67 ms (1.24 ms for mlp_f2) against the 21.3 ms block;
  no overrun in 20 s runs, the first decision at 2.21 s as the fixture says.
- Parity: `tests/vectors/extractor_mod_expected.h` is a 5 s LCG noise
  amplitude-modulated at 192 Hz in integer arithmetic, so the C and the Python
  see identical samples; `extractor_test` holds the envelope and the ten
  features to it (1280 checks), `model_parity_test` covers all 13 models (482).
  `bdtrain fixtures` regenerates the fixture and the coefficient header, and
  the training package's tests fail when either is stale.

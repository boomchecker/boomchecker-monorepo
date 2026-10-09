/**
 * @file models.h
 * @brief Every classifier_t this package's models/ directory exports.
 *
 * The registry used to carry these as bare `extern` declarations inside its own
 * .c file, which means nothing ever cross-checked them against the definitions:
 * a model file renaming its entry, or giving it a different type, would fail at
 * link time with a symbol name rather than at compile time with the mismatch.
 * Both sides include this instead.
 *
 * Adding a model is still two edits and no more: a file here, and a line in
 * src/classifier_registry.c - plus this declaration, which is what makes the
 * compiler check the pair.
 */
#ifndef BOOMDETECT_MODELS_H
#define BOOMDETECT_MODELS_H

#include "classifier.h"

extern const classifier_t classifier_mlp_v6;
extern const classifier_t classifier_svm_v3;

/* Run fw2_mlp2, 2026-10-01: the public sets plus 55 of the node's own field
   recordings (two outdoor sessions, office confusers) and four distance
   variants of each. mlp_f2 is a two-hidden-layer MLP (68 -> 32 -> 16 -> 1) on
   layout 2, shipped since 2026-10-02 at the 5 FA/h point (7.656) like the
   others. The default from 2026-10-01 to 2026-10-09; kept for rollback. */
extern const classifier_t classifier_mlp_f2;

/* Run fw4_modb, 2026-10-03: the first models on layout 4 (stats_spectral_mod,
   src/extractor_mod.c) - layout 2 plus the modulation spectrum of the 1-4 kHz
   envelope over the last two seconds, where a hovering drone's blade-pass rate
   (170-185 Hz for the Phantom 4) still shows when its spectrum is at the
   background. 67 field recordings (84 min), among them the DJI straight
   overhead at 20-90 m, outdoor backgrounds and 30 min of traffic. Judged out
   of fold at the 5 FA/h point: the DJI at 60/80/90 m goes from 38/44/17 % of
   windows (mlp_f2's family) to 86/85/71 % (gbt_m1) and 87/83/72 % (mlp_m1),
   the 30.9 take at 70 m from 0 to 78/89 %; at 1 FA/h both alarm on all seven
   2.10 heights with none of 27 negatives and no alarm in 30 min of traffic.
   gbt_m1 is the forest (200 trees, 78 inputs, threshold stable across folds
   and seeds); mlp_m1 the 78 -> 32 -> 16 -> 1 MLP (highest field AUC, but its
   1 FA/h point varies widely with the training seed). The first two seconds
   of a run give no decision: the envelope ring has to fill first. Outdoors on
   2026-10-05, the DJI hovering at 100 m: gbt_m1 called 35 of 35 windows in the
   morning, mlp_f2 1 of 31 in the afternoon. */
extern const classifier_t classifier_gbt_m1;
extern const classifier_t classifier_mlp_m1;

#endif /* BOOMDETECT_MODELS_H */

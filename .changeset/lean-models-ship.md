---
"fw-common-boomdetect": minor
"fw-bom-stm32node": minor
---

gbt_m1 is the default model, and the image carries only the models still worth switching to.

gbt_m1 - the forest on layout 4, which adds the modulation spectrum of the
1-4 kHz envelope to the spectral statistics - replaces mlp_f2 as the model
`detect` runs after a reset. It has run on the board since 4 October. Outdoors
on 5 October, with a DJI Phantom 4 hovering at 100 m, it called 35 of 35
windows (mlp_f2 called 1 of 31 that afternoon), with no alarm on the
background with people around; across five training seeds its operating point
barely moves. Its default threshold is 3.211, and the first decision of a run
comes after about 2.2 s, once the envelope ring is full.

The registry is down to five models: gbt_m1, mlp_f2 (the previous default, for
rollback), mlp_m1 (the MLP on layout 4), and mlp_v6 and svm_v3, which the
selftest and the parity harness are anchored to. gbt_f1, gbt_f2, gbt_f3,
mlp_f1, mlp_l2, gbt_l2, gbt_reg_l2 and cnn_small are gone: none was ahead of
these on the field recordings, and their tables were most of the generated
code. The layout-3 extractor and the small-network interpreter stay.
`stm32node-cli` follows the new default (model gbt_m1, threshold 3211), and the
field manual's model table, threshold ladder and outdoor procedure are written
for gbt_m1.

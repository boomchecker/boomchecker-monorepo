---
"fw-common-boomdetect": minor
"fw-bom-stm32node": minor
---

First models trained on the node's own field recordings, and a gate to match.

`mlp_f1` (MLP 68->16->1 on the stats_spectral layout) is the new default and
`gbt_f1` sits next to it. Both were trained on the public sets plus 28
recordings made with the node's microphone - a DJI Phantom 4, a Runner 250 and
nine kinds of negatives - and four distance variants of each. Judged out of
fold (every recording by a model trained without it) at 5 false-alarm windows
per hour on the public validation negatives, mlp_f1 alarmed on 14 of 17 drone
recordings and on none of 11 negatives, with 0.26 false alarms per hour on
11.6 h of held-out public negatives; the previous default mlp_v6 at its
hand-set 3.0 found 10 of 17 with 16.7. mlp_v6 and the other models stay in the
image, and moving it back to the front of the registry is the rollback.

`detect` now gates at RMS 0.003 by default instead of 0.010: in the field
recordings the background sat at 0.004 and a drone at 20 m and beyond at
0.004-0.009, so at 0.010 most of a flight never made a window.

The training package reads field sessions from `raw/field`
(`bdtrain train --field --share --folds --augment`), and `bdtrain export` can
export under a new C name and keep parity vectors for registry models of
earlier runs.

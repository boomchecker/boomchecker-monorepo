---
"fw-common-boomdetect": minor
"fw-bom-stm32node": minor
---

Move the acoustic detector out of the firmware's CubeMX-owned `Core/` into a new
shared package, `fw/common/boomdetect`, that also builds and runs natively.

Everything between microphone samples and a decision now lives there:
decimation, the sample FIFO, framing, the RMS gate, MFCC, feature aggregation
and the classifier. `fw/bom-stm32node/App/detect/` keeps the microphone, the
pacing and the console rendering, the same split `App/link/` has against
`fw/common/boomlink`. The API separates feeding samples from doing work, because
running two MFCCs in one superloop iteration overran the real-time budget and
starved USB — that used to be a comment asking the loop to behave.

The model is no longer chosen by an `#include`. Each one is a translation unit
exporting a `classifier_t`, and `model [name]` lists or selects at runtime.
Thresholds move with the model, since a linear SVM's decisions live around ±3
while an MLP's are unbounded logits.

CMSIS-DSP is fetched at tag v1.15.0 instead of being vendored, which removes 470
files and 11 MB from the tree — the 467 that were committed as the vendored copy,
plus the LICENSE, README and patch file added later to record its provenance. The one hand-edit the vendored copy carried —
RMS rather than absolute-maximum conditioning of the FFT input, which the
deployed model was selected against — is now an explicit source file with its
Apache-2.0 attribution rather than an unrecorded difference.

Host tests run under ASan and UBSan in CI. A `detselftest` command drives the
chain from a deterministic integer sequence and prints raw float bits, so a
refactor can be shown not to have moved any number; it matched bit for bit
across the whole move, and caught one real off-by-one on the way. The same
generator runs on a host, so that check no longer needs a board - though the two
sides do not produce identical bits, and each is compared against its own
recorded baseline. Decisions agree to about 1e-6.

Also raises the CLI binding limit and checks the registration result, instead of
silently dropping commands past the limit as happened once before.

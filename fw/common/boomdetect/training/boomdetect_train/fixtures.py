"""Generate the C fixtures that hold the C extractors to the Python specification.

tests/vectors/extractor_expected.h carries, for the board's LCG fixture signal:

* the 8 spectral scalars of the first frames (frame_scalars.c against
  features.frame_scalars);
* the layout-2 and layout-3 feature vectors of every window;
* per window, whether the comb fundamental was numerically stable - the C
  and the Python pick the argmax of 66 comb sums, and when the best two are
  within a hair the float32 side may pick the other. Such windows are marked
  and the f0 entries are not compared there; nothing else is discrete except
  the roll-off bin, whose one-step disagreement is inside the tolerance.

The values are float32 bit patterns like the selftest fixture, so the file is
diffable and "unchanged" means unchanged.
"""

from __future__ import annotations

import time
from pathlib import Path

import numpy as np

from boomdetect_train import export as ex
from boomdetect_train.dsp.audio import decimate3, pcm16_to_float
from boomdetect_train.dsp.mfcc import Frontend
from boomdetect_train.dsp.selftest import lcg_signal
from boomdetect_train.dsp.windows import Gate, windows
from boomdetect_train.features import (
    LAYOUT_LOGMEL,
    LAYOUT_STATS_SPECTRAL,
    N_HARMONICS,
    SAMPLE_RATE_HZ,
    _interp_mag,
    extract,
    f0_candidates,
    frame_scalars_batch,
)
from boomdetect_train.paths import VECTORS_DIR

SCALAR_FRAMES = 3
F0_STABLE_MARGIN = 1e-3  # relative gap between the best and second-best comb sum


def comb_margin(mag: np.ndarray) -> float:
    """(best - second best) / best of the harmonic comb sums for one frame."""
    cands = f0_candidates()
    sums = np.empty(cands.shape[0])
    for i, f0 in enumerate(cands):
        h = np.arange(1, N_HARMONICS + 1) * f0
        h = h[h <= SAMPLE_RATE_HZ / 2]
        sums[i] = float(_interp_mag(mag, h).sum())
    order = np.sort(sums)[::-1]
    return float((order[0] - order[1]) / order[0]) if order[0] > 0 else 0.0


def extractor_fixture_text() -> str:
    x = pcm16_to_float(decimate3(lcg_signal()))
    frames = Frontend().process(x)
    wins = windows(frames.rms, Gate.PER_FRAME, squelch=0.0)
    scal = frame_scalars_batch(frames.mag[:SCALAR_FRAMES])

    l2 = np.stack([extract(LAYOUT_STATS_SPECTRAL, frames, w.frames) for w in wins])
    l3 = np.stack([extract(LAYOUT_LOGMEL, frames, w.frames) for w in wins])
    stable = np.asarray(
        [all(comb_margin(frames.mag[f]) >= F0_STABLE_MARGIN for f in w.frames) for w in wins],
        dtype=np.int64,
    )

    guard = "BOOMDETECT_EXTRACTOR_EXPECTED_H"
    prov = [
        f"generated: {time.strftime('%Y-%m-%dT%H:%M:%S')} by bdtrain fixtures",
        "signal: the detselftest LCG (66048 samples at 48 kHz, seed 1), decimated by 3",
        f"windows: {len(wins)} of 14 frames, no squelch; scalars of the first "
        f"{SCALAR_FRAMES} frames",
        "f0 stable = every frame's best comb beats the runner-up by >= "
        f"{F0_STABLE_MARGIN:g} relative",
    ]
    lines = ex._header(guard, "Layout 2 / layout 3 features of the LCG fixture, from Python", prov)
    lines += [
        f"#define EXPECTED_SCALAR_FRAMES {SCALAR_FRAMES}",
        f"#define EXPECTED_WINDOWS {len(wins)}",
        f"#define EXPECTED_L2_WIDTH {l2.shape[1]}",
        f"#define EXPECTED_L3_WIDTH {l3.shape[1]}",
        "",
        ex._c_array("expected_scalars", "float", scal, (SCALAR_FRAMES, scal.shape[1]), ex._c_float),
        "",
        ex._c_array("expected_l2", "float", l2, l2.shape, ex._c_float),
        "",
        ex._c_array("expected_l3", "float", l3, l3.shape, ex._c_float),
        "",
        ex._c_array("expected_f0_stable", "uint8_t", stable, (len(wins),), lambda v: f"{int(v)}u"),
        "",
        f"#endif /* {guard} */",
        "",
    ]
    return "\n".join(lines)


def write_extractor_fixture(path: Path | None = None) -> Path:
    p = path if path is not None else VECTORS_DIR / "extractor_expected.h"
    return ex.write_text(p, extractor_fixture_text())

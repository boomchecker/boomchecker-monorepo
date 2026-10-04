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


# --- layout 4: the envelope filters and the modulation features --------------

ENV_COEFS_H = VECTORS_DIR.parent.parent / "src" / "envelope_coefs.h"
ENV_FRAMES = 3  # envelope rows of the first frames, for pinpointing a filter error


def envelope_coefs_text() -> str:
    """src/envelope_coefs.h: the three biquads of dsp/mfcc.py's envelope, as float32.

    Rows are scipy's sos order without a0: b0, b1, b2, a1, a2. The first two
    sections are the 1-4 kHz band-pass, the last the 400 Hz low-pass, which the C
    applies in that order with |x| in between - exactly envelope_1k().
    """
    from boomdetect_train.dsp.mfcc import (
        ENV_BAND_HZ,
        ENV_DECIM,
        ENV_LOWPASS_HZ,
        ENV_PER_FRAME,
        envelope_filters,
    )
    from boomdetect_train.features import MOD_FRAMES

    bp, lp = envelope_filters()
    sos = np.concatenate([bp, lp])
    rows = sos[:, [0, 1, 2, 4, 5]]
    assert np.allclose(sos[:, 3], 1.0), "scipy sos rows should have a0 == 1"
    guard = "BOOMDETECT_ENVELOPE_COEFS_H"
    prov = [
        f"generated: {time.strftime('%Y-%m-%dT%H:%M:%S')} by bdtrain fixtures",
        f"band-pass: Butterworth order 2, {ENV_BAND_HZ[0]:g}-{ENV_BAND_HZ[1]:g} Hz at "
        f"{SAMPLE_RATE_HZ:g} Hz (scipy.signal.butter, sos), sections 0..{bp.shape[0] - 1}",
        f"low-pass: Butterworth order 2, {ENV_LOWPASS_HZ:g} Hz, section {bp.shape[0]}",
        "a row is b0, b1, b2, a1, a2 with a0 == 1; direct form II transposed, like sosfilt",
    ]
    lines = ex._header(guard, "The envelope chain of dsp/mfcc.py, as the C runs it", prov)
    lines += [
        f"#define BOOMDETECT_ENV_SOS_SECTIONS {sos.shape[0]}u",
        f"#define BOOMDETECT_ENV_BANDPASS_SECTIONS {bp.shape[0]}u",
        f"#define BOOMDETECT_ENV_COEFS_DECIM {ENV_DECIM}u",
        f"#define BOOMDETECT_ENV_COEFS_PER_HOP {ENV_PER_FRAME}u",
        f"#define BOOMDETECT_ENV_COEFS_RING {MOD_FRAMES * ENV_PER_FRAME}u",
        "",
        ex._c_array("boomdetect_env_sos", "float", rows, rows.shape, ex._c_float),
        "",
        f"#endif /* {guard} */",
        "",
    ]
    return "\n".join(lines)


def write_envelope_coefs(path: Path | None = None) -> Path:
    return ex.write_text(path if path is not None else ENV_COEFS_H, envelope_coefs_text())


def mod_fixture_text() -> str:
    """tests/vectors/extractor_mod_expected.h: layout 4 on the AM-modulated LCG.

    The firmware's windows (disjoint runs of 14 frames, no squelch) over 5 s of
    signal; those that close before the two-second envelope ring is full are
    NaN in Python and must produce no window in C, so the header says which.
    """
    from boomdetect_train.dsp.mfcc import ENV_PER_FRAME
    from boomdetect_train.dsp.selftest import AM_INPUT_LEN, AM_PERIOD, am_lcg_signal, am_table
    from boomdetect_train.features import LAYOUT_STATS_SPECTRAL_MOD, MOD_FRAMES

    x = pcm16_to_float(decimate3(am_lcg_signal()))
    frames = Frontend().process(x)
    wins = list(windows(frames.rms, Gate.PER_FRAME, squelch=0.0))
    rows = np.stack([extract(LAYOUT_STATS_SPECTRAL_MOD, frames, w.frames) for w in wins])
    ready = np.isfinite(rows).all(axis=1)
    ends = np.asarray([w.end for w in wins], dtype=np.int64)
    assert ready.any(), "the fixture signal is too short for a full ring"
    assert all(r == (e >= MOD_FRAMES - 1) for r, e in zip(ready, ends, strict=True))
    l4 = rows[ready]
    env = frames.env[:ENV_FRAMES].reshape(-1)
    table = am_table()

    guard = "BOOMDETECT_EXTRACTOR_MOD_EXPECTED_H"
    prov = [
        f"generated: {time.strftime('%Y-%m-%dT%H:%M:%S')} by bdtrain fixtures",
        f"signal: the selftest LCG ({AM_INPUT_LEN} samples at 48 kHz, seed 1), amplitude-"
        f"modulated at 48000/{AM_PERIOD} Hz in Q10 integer arithmetic, decimated by 3",
        f"windows: {len(wins)} of 14 frames, no squelch; {int(ready.sum())} close with the "
        f"{MOD_FRAMES}-frame envelope ring full and carry features, the rest are NaN",
        f"envelope: the {ENV_PER_FRAME} samples per hop of the first {ENV_FRAMES} frames",
    ]
    lines = ex._header(guard, "Layout 4 features of the AM fixture, from Python", prov)
    lines += [
        f"#define EXPECTED_AM_INPUT_LEN {AM_INPUT_LEN}u",
        f"#define EXPECTED_AM_PERIOD {AM_PERIOD}u",
        f"#define EXPECTED_AM_WINDOWS {len(wins)}",
        f"#define EXPECTED_AM_READY {int(ready.sum())}",
        f"#define EXPECTED_L4_WIDTH {l4.shape[1]}",
        f"#define EXPECTED_ENV_FRAMES {ENV_FRAMES}",
        f"#define EXPECTED_ENV_PER_FRAME {ENV_PER_FRAME}",
        "",
        ex._c_array("expected_am_table", "int16_t", table, table.shape, lambda v: f"{int(v)}"),
        "",
        ex._c_array(
            "expected_l4_ready",
            "uint8_t",
            ready.astype(np.int64),
            (len(wins),),
            lambda v: f"{int(v)}u",
        ),
        "",
        ex._c_array(
            "expected_l4_end_frame",
            "uint16_t",
            ends[ready],
            (int(ready.sum()),),
            lambda v: f"{int(v)}u",
        ),
        "",
        ex._c_array("expected_l4", "float", l4, l4.shape, ex._c_float),
        "",
        ex._c_array("expected_env", "float", env, env.shape, ex._c_float),
        "",
        f"#endif /* {guard} */",
        "",
    ]
    return "\n".join(lines)


def write_mod_fixture(path: Path | None = None) -> Path:
    p = path if path is not None else VECTORS_DIR / "extractor_mod_expected.h"
    return ex.write_text(p, mod_fixture_text())

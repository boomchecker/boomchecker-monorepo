"""Replay a recording through the board's chain, offline, and show every window.

`bdtrain score REC.wav` is what the board's `detect` would have printed for
that audio, for every model at once, with the eight spectral scalars beside
each decision. The board tells you the score; this tells you WHY - whether the
harmonic comb found a rotor, where its f0 sat, how much energy lived above
4 kHz - which is the only way to tell "the microphone never carried the rotor"
from "the model does not know this rotor".

The chain is the firmware's: 48 kHz in, the board's 3x decimation, the same
MFCC tables, disjoint runs of 14 frames gated per frame at the same squelch.
The parity tests hold the Python to the C, so the decisions here are the
board's decisions to float rounding.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

import numpy as np
import pandas as pd

from boomdetect_train.datasets.cache import CachedFrames, frame_rows
from boomdetect_train.dsp.audio import read_audio, to_16k
from boomdetect_train.dsp.mfcc import HOP, SAMPLE_RATE_HZ, Frontend
from boomdetect_train.dsp.windows import DEFAULT_SQUELCH, Gate, windows
from boomdetect_train.evaluate import window_features
from boomdetect_train.features import LAYOUT_STATS, SCALAR_NAMES
from boomdetect_train.models.headers import SHIPPED_THRESHOLDS, shipped_models
from boomdetect_train.paths import runs_dir

F0_SCALE_HZ = 400.0  # features.py normalises f0 by this


@dataclass
class ScoredModel:
    name: str
    layout: int
    thr: float
    decisions: np.ndarray  # (windows,)


def load_models(run: str | None, names: list[str] | None):
    """(name, layout, default threshold, scorer) for the shipped models and a run's."""
    out = []
    for name, hdr in shipped_models().items():
        out.append((name, LAYOUT_STATS, float(SHIPPED_THRESHOLDS[name]), hdr.score))
    if run:
        from boomdetect_train.run import load_run_models

        run_dir = runs_dir() / run
        for name, m in load_run_models(run_dir).items():
            thr = _exported_threshold(name)
            out.append((name, int(m.layout), thr, m.score))
    if names:
        out = [o for o in out if o[0] in names]
    return out


def _exported_threshold(name: str) -> float:
    """The default_thr_milli the exporter wrote into models/model_<name>.c, if any."""
    from boomdetect_train.paths import MODELS_DIR

    p = MODELS_DIR / f"model_{name}.c"
    if not p.exists():
        return float("nan")
    for line in p.read_text(encoding="utf-8", errors="ignore").splitlines():
        if "default_thr_milli" in line and "=" in line:
            digits = "".join(ch for ch in line.split("=")[1] if ch in "-0123456789")
            if digits.strip("-"):
                return int(digits) / 1000.0
    return float("nan")


def score_recording(
    wav: Path,
    run: str | None = "full",
    models: list[str] | None = None,
    squelch: float = DEFAULT_SQUELCH,
) -> tuple[pd.DataFrame, list[ScoredModel]]:
    """One row per firmware window: time, level, the scalars, a column per model."""
    x, sr = read_audio(wav)
    x16 = to_16k(x, sr)
    cf = CachedFrames(frame_rows(Frontend().process(x16)))
    wins = windows(cf.rms, Gate.PER_FRAME, squelch)

    rows = []
    for w in wins:
        sc = cf.scalars[w.frames].mean(axis=0)
        row = {
            "t_end_s": (w.end * HOP + 1024) / SAMPLE_RATE_HZ,
            "rms": float(np.median(cf.rms[w.frames])),
        }
        for k, nm in enumerate(SCALAR_NAMES):
            row[nm] = float(sc[k])
        row["f0_hz"] = row.pop("f0") * F0_SCALE_HZ
        rows.append(row)
    df = pd.DataFrame(rows)

    scored: list[ScoredModel] = []
    for name, layout, thr, scorer in load_models(run, models):
        if wins:
            feats = np.stack([window_features(layout, cf, w.frames) for w in wins]).astype(
                np.float32
            )
            dec = np.asarray(scorer(feats), dtype=np.float32).reshape(-1)
        else:
            dec = np.empty(0, np.float32)
        df[name] = dec
        scored.append(ScoredModel(name, layout, thr, dec))

    total_s = len(x16) / SAMPLE_RATE_HZ
    df.attrs["seconds"] = total_s
    df.attrs["frames"] = int(cf.n)
    df.attrs["gated_frames"] = int((cf.rms < squelch).sum())
    return df, scored


def render(df: pd.DataFrame, scored: list[ScoredModel], wav: Path) -> str:
    n = len(df)
    lines = [
        f"{wav.name}: {df.attrs['seconds']:.1f} s, {df.attrs['frames']} frames, "
        f"{df.attrs['gated_frames']} below squelch, {n} windows",
        "",
    ]
    if n == 0:
        lines.append("no window cleared the gate - the recording is below squelch throughout")
        return "\n".join(lines)

    lines += [
        "| model | thr | windows >= thr | median | p90 | max |",
        "|---|---|---|---|---|---|",
    ]
    for s in scored:
        d = s.decisions
        over = int((d >= s.thr).sum()) if np.isfinite(s.thr) else 0
        thr = f"{s.thr:.2f}" if np.isfinite(s.thr) else "-"
        lines.append(
            f"| {s.name} | {thr} | {over}/{n} | {np.median(d):.2f} | "
            f"{np.percentile(d, 90):.2f} | {d.max():.2f} |"
        )

    lines += [
        "",
        "scalars over the windows (median [p10..p90]):",
    ]
    for nm in ("rms", "hi_ratio", "mid_ratio", "flatness", "harmonicity", "f0_hz", "centroid"):
        v = df[nm].to_numpy()
        lines.append(
            f"  {nm:12s} {np.median(v):8.3f}  [{np.percentile(v, 10):8.3f} .. "
            f"{np.percentile(v, 90):8.3f}]"
        )
    return "\n".join(lines)

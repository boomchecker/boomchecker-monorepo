"""Scoring windows the way the board does, and turning the scores into numbers.

Every model is judged on identical windows: the firmware's gating policy
(disjoint runs of 14 accepted frames, per-frame RMS gate) over the cached
frames of each clip, one decision per window. The metrics are the ones the
project has actually needed and never had in one place:

* window AUC - is the score ordering positives above negatives at all;
* file-level detection and false-alarm rates at a threshold, with the alarm
  rule the board will run (K-of-N) or the bare ">= m windows" rule;
* false alarms per hour of negative audio - the number a field deployment is
  judged by, which a percentage of windows hides;
* the "champion" operating point: the threshold that alarms on zero negative
  files while detecting the most positive files, the rule the deployed model
  was picked with, so the comparison is apples to apples.

Suites are named subsets of the manifest (val, halmstad, salford, real_mic,
stress) so a report can say where a model wins rather than only whether.
"""

from __future__ import annotations

from collections.abc import Callable, Iterable
from dataclasses import dataclass, field

import numpy as np
import pandas as pd
from sklearn.metrics import roc_auc_score

from boomdetect_train.datasets.cache import CachedFrames, FrameCache
from boomdetect_train.datasets.manifest import ROLE_REAL, ROLE_STRESS, ROLE_TRAIN, ROLE_UNSEEN
from boomdetect_train.decision import KofN, clip_alarmed
from boomdetect_train.dsp.windows import DEFAULT_SQUELCH, Gate, window_seconds, windows
from boomdetect_train.features import (
    LAYOUT_LOGMEL,
    LAYOUT_STATS,
    LAYOUT_STATS_SPECTRAL,
    logmel_patch,
    stats52,
    stats_spectral_from_scalars,
)

Scorer = Callable[[np.ndarray], np.ndarray]


def window_features(layout: int, cf: CachedFrames, idx: np.ndarray) -> np.ndarray:
    """A feature vector of `layout` from cached frames (no magnitude spectra needed)."""
    if layout == LAYOUT_STATS:
        return stats52(cf.mfcc[idx])
    if layout == LAYOUT_STATS_SPECTRAL:
        return stats_spectral_from_scalars(cf.mfcc[idx], cf.scalars[idx], cf.logmel[idx])
    if layout == LAYOUT_LOGMEL:
        return logmel_patch(cf.logmel[idx])
    raise ValueError(f"unknown layout {layout}")


def clip_windows(
    cf: CachedFrames,
    layout: int,
    gate: Gate = Gate.PER_FRAME,
    squelch: float | None = DEFAULT_SQUELCH,
    hop: int | None = None,
) -> tuple[np.ndarray, np.ndarray]:
    """(features, end_frames) for every window of one clip under `gate`."""
    wins = windows(cf.rms, gate, squelch, hop=hop)
    if not wins:
        return np.empty((0, 0), dtype=np.float32), np.empty((0,), dtype=np.int64)
    feats = np.stack([window_features(layout, cf, w.frames) for w in wins]).astype(np.float32)
    ends = np.asarray([w.end for w in wins], dtype=np.int64)
    return feats, ends


@dataclass
class ClipScores:
    id: str
    source: str
    label: int
    category: str
    role: str
    split: str
    duration: float
    decisions: np.ndarray  # (windows,) float32
    ends: np.ndarray  # (windows,) frame index that closed each window


@dataclass
class Suite:
    name: str
    clips: list[ClipScores] = field(default_factory=list)

    @property
    def labels(self) -> np.ndarray:
        return np.asarray([c.label for c in self.clips], dtype=np.int64)

    def negative_hours(self) -> float:
        return sum(c.duration for c in self.clips if c.label == 0) / 3600.0


SUITES = {
    # name: (roles, splits, sources) - None means any
    "val": ((ROLE_TRAIN,), ("val",), None),
    "halmstad": ((ROLE_UNSEEN,), None, ("halmstad",)),
    "salford": ((ROLE_UNSEEN,), None, ("salford",)),
    "real_mic": ((ROLE_REAL,), None, ("own_recordings",)),
    "stress": ((ROLE_STRESS,), None, None),
}


def select_suite(manifest: pd.DataFrame, name: str) -> pd.DataFrame:
    roles, splits, sources = SUITES[name]
    m = manifest["role"].isin(roles)
    if splits is not None:
        m &= manifest["split"].isin(splits)
    if sources is not None:
        m &= manifest["source"].isin(sources)
    return manifest[m]


def score_clips(
    rows: pd.DataFrame,
    cache: FrameCache,
    layout: int,
    scorer: Scorer,
    *,
    gate: Gate = Gate.PER_FRAME,
    squelch: float | None = DEFAULT_SQUELCH,
) -> list[ClipScores]:
    out: list[ClipScores] = []
    for rec in rows.itertuples(index=False):
        cf = cache.get(rec.source, rec.id)
        feats, ends = clip_windows(cf, layout, gate, squelch)
        dec = scorer(feats).astype(np.float32).reshape(-1) if feats.shape[0] else np.empty(0)
        out.append(
            ClipScores(
                id=rec.id,
                source=rec.source,
                label=int(rec.label),
                category=str(rec.category),
                role=str(rec.role),
                split=str(rec.split),
                duration=float(rec.duration),
                decisions=dec.astype(np.float32),
                ends=ends,
            )
        )
    return out


# --- metrics ------------------------------------------------------------------


def window_auc(clips: Iterable[ClipScores]) -> float:
    """AUC over every window, each carrying its clip's label. NaN if one class is absent."""
    y, s = [], []
    for c in clips:
        y.extend([c.label] * c.decisions.shape[0])
        s.extend(c.decisions.tolist())
    if not y or len(set(y)) < 2:
        return float("nan")
    return float(roc_auc_score(np.asarray(y), np.asarray(s)))


def clip_verdicts(
    clips: Iterable[ClipScores], threshold: float, rule: KofN | None, min_windows: int = 1
) -> np.ndarray:
    """Per-clip alarm verdicts. Without a rule, a clip alarms on >= min_windows drone windows."""
    out = []
    for c in clips:
        if rule is None:
            out.append(bool((c.decisions >= threshold).sum() >= min_windows))
        else:
            out.append(clip_alarmed(c.decisions, threshold, rule))
    return np.asarray(out, dtype=bool)


@dataclass
class OperatingPoint:
    threshold: float
    detected: int  # positive clips that alarmed
    positives: int
    false_alarms: int  # negative clips that alarmed
    negatives: int
    fa_windows_per_hour: float  # drone-called windows per hour of negative audio

    @property
    def detection_rate(self) -> float:
        return self.detected / self.positives if self.positives else float("nan")

    @property
    def false_alarm_rate(self) -> float:
        return self.false_alarms / self.negatives if self.negatives else float("nan")


def operating_point(
    clips: list[ClipScores], threshold: float, rule: KofN | None, min_windows: int = 1
) -> OperatingPoint:
    labels = np.asarray([c.label for c in clips])
    verdict = clip_verdicts(clips, threshold, rule, min_windows)
    neg_hours = sum(c.duration for c in clips if c.label == 0) / 3600.0
    fa_windows = sum(int((c.decisions >= threshold).sum()) for c in clips if c.label == 0)
    return OperatingPoint(
        threshold=float(threshold),
        detected=int((verdict & (labels == 1)).sum()),
        positives=int((labels == 1).sum()),
        false_alarms=int((verdict & (labels == 0)).sum()),
        negatives=int((labels == 0).sum()),
        fa_windows_per_hour=fa_windows / neg_hours if neg_hours > 0 else float("nan"),
    )


def threshold_grid(clips: list[ClipScores], n: int = 200) -> np.ndarray:
    """Candidate thresholds spanning the observed decisions."""
    all_d = np.concatenate([c.decisions for c in clips if c.decisions.shape[0]] or [np.zeros(1)])
    lo, hi = float(np.min(all_d)), float(np.max(all_d))
    if not np.isfinite(lo) or not np.isfinite(hi) or hi <= lo:
        return np.asarray([lo], dtype=np.float64)
    return np.linspace(lo, hi, n)


def champion_threshold(
    clips: list[ClipScores], rule: KofN | None, min_windows: int = 2
) -> OperatingPoint | None:
    """The deployed model's selection rule: zero alarmed negatives, then most detected positives.

    Ties go to the LOWER threshold (more sensitive), then the sweep is done.
    Returns None when no threshold silences every negative.
    """
    best: OperatingPoint | None = None
    for thr in threshold_grid(clips):
        op = operating_point(clips, float(thr), rule, min_windows)
        if op.false_alarms > 0:
            continue
        if best is None or op.detected > best.detected:
            best = op
    return best


def threshold_at_fa_rate(
    clips: list[ClipScores], max_fa_per_hour: float, rule: KofN | None
) -> OperatingPoint | None:
    """Lowest threshold whose drone-called windows on negatives stay under a rate per hour."""
    best = None
    for thr in threshold_grid(clips):
        op = operating_point(clips, float(thr), rule)
        if np.isnan(op.fa_windows_per_hour) or op.fa_windows_per_hour <= max_fa_per_hour:
            if best is None or thr < best.threshold:
                best = op
    return best


def windows_per_hour() -> float:
    """Windows a continuous, un-squelched signal yields per hour (about 8036)."""
    return 3600.0 / window_seconds()


@dataclass
class WindowRates:
    """Window-level numbers at a threshold: the clip-length-independent view."""

    threshold: float
    pos_windows: int
    neg_windows: int
    tpr: float  # positive windows called drone / positive windows
    fa_per_hour: float  # negative windows called drone per hour of negative audio
    neg_hours: float


def window_rates(clips: list[ClipScores], threshold: float) -> WindowRates:
    pos = np.concatenate([c.decisions for c in clips if c.label == 1] or [np.empty(0)])
    neg = np.concatenate([c.decisions for c in clips if c.label == 0] or [np.empty(0)])
    neg_hours = sum(c.duration for c in clips if c.label == 0) / 3600.0
    fired = int((neg >= threshold).sum())
    return WindowRates(
        threshold=float(threshold),
        pos_windows=int(pos.shape[0]),
        neg_windows=int(neg.shape[0]),
        tpr=float((pos >= threshold).mean()) if pos.shape[0] else float("nan"),
        fa_per_hour=fired / neg_hours if neg_hours > 0 else float("nan"),
        neg_hours=neg_hours,
    )


def threshold_for_fa_rate(clips: list[ClipScores], max_fa_per_hour: float) -> float:
    """Lowest threshold at which negative windows fire at most `max_fa_per_hour` times per hour.

    Exact rather than swept: with the negative decisions sorted, the allowed
    count of firings is the budget times the negative hours, and the threshold
    is one float32 step above the first decision that must not fire.
    """
    neg = np.concatenate([c.decisions for c in clips if c.label == 0] or [np.empty(0)])
    neg_hours = sum(c.duration for c in clips if c.label == 0) / 3600.0
    if neg.shape[0] == 0 or neg_hours <= 0:
        return float("nan")
    allowed = int(np.floor(max_fa_per_hour * neg_hours))
    desc = np.sort(neg.astype(np.float32))[::-1]
    if allowed >= desc.shape[0]:
        return float(np.nextafter(desc[-1], -np.inf))
    return float(np.nextafter(desc[allowed], np.float32(np.inf)))


@dataclass
class ClipRates:
    """Clip-level alarm numbers, over the clips long enough for the rule to apply."""

    eligible_pos: int
    eligible_neg: int
    detected: int
    false_alarms: int

    @property
    def detection_rate(self) -> float:
        return self.detected / self.eligible_pos if self.eligible_pos else float("nan")


def clip_rates(clips: list[ClipScores], threshold: float, rule: KofN | None) -> ClipRates:
    need = rule.n if rule is not None else 1
    elig = [c for c in clips if c.decisions.shape[0] >= need]
    verdict = clip_verdicts(elig, threshold, rule)
    labels = np.asarray([c.label for c in elig], dtype=np.int64)
    return ClipRates(
        eligible_pos=int((labels == 1).sum()),
        eligible_neg=int((labels == 0).sum()),
        detected=int((verdict & (labels == 1)).sum()),
        false_alarms=int((verdict & (labels == 0)).sum()),
    )


def per_category_false_alarms(clips: list[ClipScores], threshold: float) -> pd.DataFrame:
    """Which negative categories fire: drone-called windows and clips per category."""
    rows = []
    for c in clips:
        if c.label != 0:
            continue
        fired = int((c.decisions >= threshold).sum())
        rows.append({"category": c.category, "windows": c.decisions.shape[0], "fired": fired})
    if not rows:
        return pd.DataFrame(columns=["category", "clips", "windows", "fired", "fired_pct"])
    df = pd.DataFrame(rows)
    g = df.groupby("category").agg(
        clips=("fired", "size"), windows=("windows", "sum"), fired=("fired", "sum")
    )
    g["fired_pct"] = 100.0 * g["fired"] / g["windows"].clip(lower=1)
    return g.sort_values("fired_pct", ascending=False).reset_index()

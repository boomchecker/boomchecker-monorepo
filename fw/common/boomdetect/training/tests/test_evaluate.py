"""Metrics on hand-made clip scores: thresholds, rates and eligibility."""

from __future__ import annotations

import numpy as np

from boomdetect_train.decision import KofN
from boomdetect_train.evaluate import (
    ClipScores,
    clip_rates,
    per_category_false_alarms,
    threshold_for_fa_rate,
    window_auc,
    window_rates,
)


def clip(cid, label, decisions, duration=1.0, category="x"):
    d = np.asarray(decisions, dtype=np.float32)
    return ClipScores(
        id=cid,
        source="t",
        label=label,
        category=category,
        role="train",
        split="val",
        duration=duration,
        decisions=d,
        ends=np.arange(d.shape[0]) * 14 + 13,
    )


def test_threshold_for_fa_rate_is_exact():
    # 3600 s of negatives = 1 hour with 4 windows; a budget of 1/hour allows
    # exactly one firing, so the threshold sits just above the second-largest.
    clips = [clip("n", 0, [0.1, 0.9, 0.5, 0.3], duration=3600.0)]
    thr = threshold_for_fa_rate(clips, 1.0)
    fired = (clips[0].decisions >= thr).sum()
    assert fired == 1
    assert 0.5 < thr <= np.nextafter(np.float32(0.5), np.float32(np.inf))
    # A zero budget silences everything.
    thr0 = threshold_for_fa_rate(clips, 0.0)
    assert (clips[0].decisions >= thr0).sum() == 0
    # A generous budget lets everything through.
    thr_all = threshold_for_fa_rate(clips, 100.0)
    assert (clips[0].decisions >= thr_all).sum() == 4


def test_window_rates_and_auc():
    clips = [
        clip("p", 1, [2.0, 3.0, -1.0], duration=3.0),
        clip("n", 0, [-2.0, 0.5], duration=7200.0),
    ]
    wr = window_rates(clips, 0.0)
    assert wr.pos_windows == 3 and wr.neg_windows == 2
    assert abs(wr.tpr - 2 / 3) < 1e-9
    assert abs(wr.fa_per_hour - 0.5) < 1e-9  # one firing in two hours
    assert window_auc(clips) > 0.8


def test_clip_rates_only_count_eligible_clips():
    rule = KofN(n=4, k_on=2, k_off=1)
    clips = [
        clip("short_pos", 1, [5.0]),  # one window: not eligible under 4
        clip("long_pos", 1, [5.0, 5.0, 0.0, 0.0, 0.0]),
        clip("long_neg", 0, [0.0, 0.0, 0.0, 0.0, 0.0]),
        clip("long_neg_fires", 0, [5.0, 0.0, 5.0, 0.0]),
    ]
    cr = clip_rates(clips, 1.0, rule)
    assert cr.eligible_pos == 1 and cr.eligible_neg == 2
    assert cr.detected == 1 and cr.false_alarms == 1
    cr1 = clip_rates(clips, 1.0, None)
    assert cr1.eligible_pos == 2 and cr1.detected == 2


def test_per_category_false_alarms_sorts_by_rate():
    clips = [
        clip("a", 0, [1.0, 1.0, 0.0], category="fan"),
        clip("b", 0, [0.0, 0.0, 0.0, 0.0], category="rain"),
        clip("c", 1, [1.0], category="drone"),
    ]
    df = per_category_false_alarms(clips, 0.5)
    assert list(df["category"]) == ["fan", "rain"]
    assert int(df.loc[0, "fired"]) == 2

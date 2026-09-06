"""Evaluate a set of models on every suite and render the comparison.

The operating point is chosen once, on the validation split of the training
data, as the lowest threshold whose false-alarm rate on negative windows stays
under a budget per hour, and is then applied unchanged to the unseen suites.
Choosing it on the unseen data would be selecting against the test set - the
trap the research-branch scripts warned about, kept structural here.

Two views at that threshold, because the suites have very different clips:

* window level (every suite): true-positive rate over positive windows and
  false alarms per hour of negative audio. Independent of clip length, so a
  0.5 s clip and a 30 s flyover weigh by their audio, not by their count.
* clip level (K-of-N alarm, clips with at least N windows): detected positive
  clips and alarmed negative clips. This is what a field deployment sees; the
  half-second training clips are too short to enter it and are not counted.
"""

from __future__ import annotations

import math
from collections.abc import Callable
from dataclasses import dataclass

import numpy as np
import pandas as pd

from boomdetect_train.datasets.cache import FrameCache
from boomdetect_train.decision import KofN
from boomdetect_train.evaluate import (
    SUITES,
    ClipScores,
    clip_rates,
    per_category_false_alarms,
    score_clips,
    select_suite,
    threshold_for_fa_rate,
    window_auc,
    window_rates,
)
from boomdetect_train.features import LAYOUT_STATS
from boomdetect_train.models.headers import SHIPPED_THRESHOLDS, shipped_models

DEFAULT_FA_PER_HOUR = 5.0


def parse_rule(text: str | None) -> KofN | None:
    """'2of4' -> KofN(n=4, k_on=2, k_off=1); 'none' or None -> no rule."""
    if text is None or text.lower() == "none":
        return None
    k, n = text.lower().split("of")
    k, n = int(k), int(n)
    return KofN(n=n, k_on=k, k_off=max(1, k - 1))


@dataclass
class ModelUnderTest:
    name: str
    layout: int
    scorer: Callable[[np.ndarray], np.ndarray]
    default_threshold: float | None = None


def shipped_models_under_test() -> list[ModelUnderTest]:
    return [
        ModelUnderTest(name, LAYOUT_STATS, m.score, SHIPPED_THRESHOLDS[name])
        for name, m in shipped_models().items()
    ]


def score_all_suites(
    manifest: pd.DataFrame, cache: FrameCache, model: ModelUnderTest
) -> dict[str, list[ClipScores]]:
    out: dict[str, list[ClipScores]] = {}
    for suite in SUITES:
        rows = select_suite(manifest, suite)
        rows = rows[[cache.has(s) for s in rows["source"]]]
        if rows.empty:
            continue
        out[suite] = score_clips(rows, cache, model.layout, model.scorer)
    return out


def _fmt(v, nd: int = 3) -> str:
    if v is None or (isinstance(v, float) and math.isnan(v)):
        return "-"
    return f"{v:.{nd}f}"


def evaluate_model(
    manifest: pd.DataFrame,
    cache: FrameCache,
    model: ModelUnderTest,
    rule: KofN | None,
    fa_per_hour: float = DEFAULT_FA_PER_HOUR,
) -> tuple[pd.DataFrame, float, dict[str, list[ClipScores]]]:
    """Per-suite rows for one model at the threshold chosen on `val`."""
    scored = score_all_suites(manifest, cache, model)
    thr = float("nan")
    if "val" in scored:
        thr = threshold_for_fa_rate(scored["val"], fa_per_hour)
    if math.isnan(thr):
        thr = model.default_threshold if model.default_threshold is not None else 0.0
    rows = []
    for suite, clips in scored.items():
        wr = window_rates(clips, thr)
        cr = clip_rates(clips, thr, rule)
        rows.append(
            {
                "model": model.name,
                "suite": suite,
                "pos_win": wr.pos_windows,
                "neg_win": wr.neg_windows,
                "auc": window_auc(clips),
                "thr": thr,
                "win_tpr": wr.tpr,
                "fa_per_h": wr.fa_per_hour,
                "clip_det": (f"{cr.detected}/{cr.eligible_pos}" if cr.eligible_pos else "-"),
                "clip_fa": f"{cr.false_alarms}/{cr.eligible_neg}" if cr.eligible_neg else "-",
            }
        )
    return pd.DataFrame(rows), thr, scored


COLUMNS = [
    "model",
    "suite",
    "pos_win",
    "neg_win",
    "auc",
    "thr",
    "win_tpr",
    "fa_per_h",
    "clip_det",
    "clip_fa",
]


def render_table(df: pd.DataFrame) -> str:
    head = "| " + " | ".join(COLUMNS) + " |"
    sep = "|" + "|".join("---" for _ in COLUMNS) + "|"
    lines = [head, sep]
    for r in df.itertuples(index=False):
        lines.append(
            "| "
            + " | ".join(
                [
                    r.model,
                    r.suite,
                    str(r.pos_win),
                    str(r.neg_win),
                    _fmt(r.auc),
                    _fmt(r.thr, 2),
                    _fmt(r.win_tpr, 2),
                    _fmt(r.fa_per_h, 1),
                    r.clip_det,
                    r.clip_fa,
                ]
            )
            + " |"
        )
    return "\n".join(lines)


def render_report(
    title: str,
    manifest: pd.DataFrame,
    cache: FrameCache,
    models: list[ModelUnderTest],
    rule: KofN | None,
    fa_per_hour: float = DEFAULT_FA_PER_HOUR,
) -> str:
    parts = [f"# {title}", ""]
    rule_txt = f"{rule.k_on}-of-{rule.n} (off below {rule.k_off})" if rule else ">= 1 window"
    parts += [
        f"Threshold per model: lowest value with at most {fa_per_hour:g} false-alarm windows per "
        "hour on the `val` negatives, then applied unchanged to every other suite. "
        "`win_tpr` = positive windows called drone; `fa_per_h` = negative windows called drone "
        f"per hour of negative audio; `clip_det` / `clip_fa` = clips alarmed under the {rule_txt} "
        "rule, over the clips long enough for it (detected/positives, alarmed/negatives).",
        "",
    ]
    tables = []
    details = []
    for m in models:
        df, thr, scored = evaluate_model(manifest, cache, m, rule, fa_per_hour)
        tables.append(df)
        if "stress" in scored:
            details.append(
                (f"{m.name}: stress probes", per_category_false_alarms(scored["stress"], thr))
            )
        if "val" in scored:
            cat = per_category_false_alarms(scored["val"], thr)
            cat = cat[cat["fired"] > 0].head(8)
            if not cat.empty:
                details.append((f"{m.name}: val negatives that fire", cat))
    if tables:
        parts += [render_table(pd.concat(tables, ignore_index=True)), ""]
    for name, cat in details:
        parts += [f"## {name}", ""]
        parts += ["| category | clips | windows | fired | fired % |", "|---|---|---|---|---|"]
        for r in cat.itertuples(index=False):
            parts.append(
                f"| {r.category} | {r.clips} | {r.windows} | {r.fired} | {r.fired_pct:.1f} |"
            )
        parts.append("")
    return "\n".join(parts)


def baseline_report(
    manifest: pd.DataFrame,
    cache: FrameCache,
    rule: str | None = "2of4",
    fa_per_hour: float = DEFAULT_FA_PER_HOUR,
) -> str:
    return render_report(
        "Baseline: the shipped models",
        manifest,
        cache,
        shipped_models_under_test(),
        parse_rule(rule),
        fa_per_hour,
    )


def compare_report(
    manifest: pd.DataFrame,
    cache: FrameCache,
    run_dir,
    rule: str | None = "2of4",
    fa_per_hour: float = DEFAULT_FA_PER_HOUR,
) -> str:
    from boomdetect_train.run import load_run_models

    models = shipped_models_under_test()
    for name, sm in load_run_models(run_dir).items():
        models.append(ModelUnderTest(name, sm.layout, sm.score))
    return render_report(
        f"Comparison: {run_dir.name}", manifest, cache, models, parse_rule(rule), fa_per_hour
    )

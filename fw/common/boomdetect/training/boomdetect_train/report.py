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

The field recordings get a section of their own: per drone, per recording, at
the board's squelch and at the lower one field use needs. A run that trained on
them is scored there by its fold models, each recording by the model that did
not see it; a run that trained on them without folds is marked in-sample,
because its field numbers then measure memory.
"""

from __future__ import annotations

import dataclasses
import math
from collections.abc import Callable
from dataclasses import dataclass

import numpy as np
import pandas as pd

from boomdetect_train.datasets.cache import FrameCache
from boomdetect_train.decision import KofN, clip_alarmed
from boomdetect_train.evaluate import (
    SUITES,
    ClipScores,
    clip_rates,
    clip_windows,
    per_category_false_alarms,
    score_clips,
    score_clips_by_fold,
    select_suite,
    threshold_for_fa_rate,
    window_auc,
    window_rates,
)
from boomdetect_train.features import LAYOUT_STATS
from boomdetect_train.models.headers import SHIPPED_THRESHOLDS, shipped_models

DEFAULT_FA_PER_HOUR = 5.0
# The board's squelch, and the one the 2026-09-25 field recordings showed the
# field needs: at 0.010 most of a drone at 20 m and beyond never makes a window.
FIELD_SQUELCHES = (0.010, 0.003)


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
    # A run with folds: the same model retrained without each fold, and the
    # fold of every field recording, so the field suite is scored out of fold.
    fold_scorers: dict[int, Callable[[np.ndarray], np.ndarray]] | None = None
    fold_of_group: dict[str, int] | None = None
    # Trained on the field recordings with no folds to judge them by.
    field_in_sample: bool = False

    @property
    def field_scoring(self) -> str:
        if self.fold_scorers:
            return "out of fold"
        return "IN-SAMPLE" if self.field_in_sample else "unseen"


def shipped_models_under_test() -> list[ModelUnderTest]:
    return [
        ModelUnderTest(name, LAYOUT_STATS, m.score, SHIPPED_THRESHOLDS[name])
        for name, m in shipped_models().items()
    ]


def _suite_rows(manifest: pd.DataFrame, cache: FrameCache, suite: str) -> pd.DataFrame:
    rows = select_suite(manifest, suite)
    return rows[[cache.has(s) for s in rows["source"]]]


def score_suite(
    manifest: pd.DataFrame,
    cache: FrameCache,
    model: ModelUnderTest,
    suite: str,
    squelch: float | None = None,
) -> list[ClipScores]:
    """One suite scored by `model`; the field suite out of fold when the model has folds."""
    rows = _suite_rows(manifest, cache, suite)
    if rows.empty:
        return []
    kw = {} if squelch is None else {"squelch": squelch}
    if suite == "field" and model.fold_scorers:
        return score_clips_by_fold(
            rows, cache, model.layout, model.fold_scorers, model.fold_of_group, **kw
        )
    return score_clips(rows, cache, model.layout, model.scorer, **kw)


def score_all_suites(
    manifest: pd.DataFrame, cache: FrameCache, model: ModelUnderTest
) -> dict[str, list[ClipScores]]:
    out: dict[str, list[ClipScores]] = {}
    for suite in SUITES:
        clips = score_suite(manifest, cache, model, suite)
        if clips:
            out[suite] = clips
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
                "win_neg_ok": wr.neg_rejected,
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
    "neg_ok_%",
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
                    _fmt(100.0 * r.win_neg_ok, 2),
                    _fmt(r.fa_per_h, 1),
                    r.clip_det,
                    r.clip_fa,
                ]
            )
            + " |"
        )
    return "\n".join(lines)


def _decisions(clips: list[ClipScores]) -> np.ndarray:
    return np.concatenate([c.decisions for c in clips] or [np.empty(0, np.float32)])


def _auc(pos: np.ndarray, neg: np.ndarray) -> float:
    if pos.shape[0] == 0 or neg.shape[0] == 0:
        return float("nan")
    from sklearn.metrics import roc_auc_score

    y = np.r_[np.ones(pos.shape[0]), np.zeros(neg.shape[0])]
    return float(roc_auc_score(y, np.r_[pos, neg]))


def _recordings_alarmed(
    clips: list[ClipScores], group_of: dict[str, str], thr: float, rule: KofN | None
) -> tuple[int, int]:
    """(recordings that alarmed, recordings) - a recording alarms if any of its runs does."""
    by_group: dict[str, bool] = {}
    for c in clips:
        g = group_of.get(c.id, c.id)
        fired = clip_alarmed(c.decisions, thr, rule) if c.decisions.shape[0] else False
        by_group[g] = by_group.get(g, False) or fired
    return sum(by_group.values()), len(by_group)


def field_stats(
    clips: list[ClipScores], group_of: dict[str, str], thr: float, rule: KofN | None
) -> dict:
    """Per drone and for the negatives: window AUC, windows called, recordings alarmed."""
    neg = [c for c in clips if c.label == 0]
    neg_d = _decisions(neg)
    out: dict = {"neg": {}, "drones": {}}
    a, n = _recordings_alarmed(neg, group_of, thr, rule)
    out["neg"] = {
        "windows": int(neg_d.shape[0]),
        "called": float((neg_d >= thr).mean()) if neg_d.shape[0] else float("nan"),
        "alarmed": f"{a}/{n}",
    }
    for drone in sorted({c.category for c in clips if c.label == 1}):
        pos = [c for c in clips if c.label == 1 and c.category == drone]
        pos_d = _decisions(pos)
        a, n = _recordings_alarmed(pos, group_of, thr, rule)
        out["drones"][drone] = {
            "windows": int(pos_d.shape[0]),
            "auc": _auc(pos_d, neg_d),
            "called": float((pos_d >= thr).mean()) if pos_d.shape[0] else float("nan"),
            "alarmed": f"{a}/{n}",
        }
    return out


def fold_thresholds(
    manifest: pd.DataFrame, cache: FrameCache, model: ModelUnderTest, fa_per_hour: float
) -> dict[int, float]:
    """Each fold model's own operating point, chosen on the `val` negatives like any other.

    A fold model is a different model; the deployed one's threshold would be
    chosen for it on `val`, so the fold model's is too. Only the negatives
    decide a false-alarm budget, which keeps this cheap.
    """
    rows = _suite_rows(manifest, cache, "val")
    rows = rows[rows["label"] == 0]
    feats = [clip_windows(cache.get(r.source, r.id), model.layout)[0] for r in rows.itertuples()]
    feats = [f for f in feats if f.shape[0]]
    if not feats or not model.fold_scorers:
        return {}
    x = np.concatenate(feats).astype(np.float32)
    seconds = float(rows["duration"].sum())
    out: dict[int, float] = {}
    for k, scorer in model.fold_scorers.items():
        d = np.asarray(scorer(x), dtype=np.float32).reshape(-1)
        pooled = ClipScores("val", "val", 0, "", "", "val", seconds, d, np.empty(0, np.int64))
        out[k] = threshold_for_fa_rate([pooled], fa_per_hour)
    return out


def _relative_to_fold_thresholds(
    clips: list[ClipScores], model: ModelUnderTest, group_of: dict[str, str], thr: dict[int, float]
) -> list[ClipScores]:
    """Decisions minus the threshold of the fold that scored them, so 0 is every fold's cut."""
    out = []
    for c in clips:
        k = model.fold_of_group[group_of.get(c.id, c.id)]
        shifted = (c.decisions - np.float32(thr[k])).astype(np.float32)
        out.append(dataclasses.replace(c, decisions=shifted))
    return out


def render_field_section(
    manifest: pd.DataFrame,
    cache: FrameCache,
    models: list[tuple[ModelUnderTest, float]],
    rule: KofN | None,
    fa_per_hour: float = DEFAULT_FA_PER_HOUR,
) -> list[str]:
    """The field recordings, per drone and per recording, at FIELD_SQUELCHES."""
    rows = _suite_rows(manifest, cache, "field")
    if rows.empty:
        return []
    group_of = dict(zip(rows["id"], rows["group"], strict=True))
    drones = sorted(rows.loc[rows["label"] == 1, "category"].unique())
    minutes = rows["duration"].sum() / 60.0
    parts = [
        "## Field recordings (the node's microphone, real drones)",
        "",
        f"{rows['group'].nunique()} recordings, {minutes:.1f} min. Threshold as "
        "above (from `val`); a model scored out of fold uses each fold model's own `val` "
        "threshold, shown as the range over the folds. `AUC <drone>` = that drone's windows "
        "against every field negative window; `win %` = windows called drone; `recs` = "
        "recordings in which the alarm rule fired. `scoring`: out of fold = each recording "
        "judged by the fold model that did not see it; unseen = the model never trained on "
        "field audio; IN-SAMPLE = trained on these very recordings, so the numbers are fitted.",
        "",
    ]
    head = ["model", "scoring", "squelch", "thr"]
    for d in drones:
        head += [f"AUC {d}", f"{d} win %", f"{d} recs"]
    head += ["neg win %", "neg recs"]
    parts += ["| " + " | ".join(head) + " |", "|" + "|".join("---" for _ in head) + "|"]
    # Keyed by the object, not the name: a run model may share a shipped model's name.
    fold_thr: dict[int, dict[int, float]] = {}
    for m, thr in models:
        if m.fold_scorers:
            ft = fold_thresholds(manifest, cache, m, fa_per_hour)
            fold_thr[id(m)] = {k: (v if np.isfinite(v) else thr) for k, v in ft.items()}
    per_rec: dict[str, dict[str, str]] = {}
    for sq in FIELD_SQUELCHES:
        for m, thr in models:
            clips = score_suite(manifest, cache, m, "field", squelch=sq)
            thr_txt = _fmt(thr, 2)
            ft = fold_thr.get(id(m))
            if ft:
                clips = _relative_to_fold_thresholds(clips, m, group_of, ft)
                thr_txt = f"{min(ft.values()):.2f}..{max(ft.values()):.2f}"
                thr = 0.0
            st = field_stats(clips, group_of, thr, rule)
            cells = [m.name, m.field_scoring, f"{sq:g}", thr_txt]
            for d in drones:
                s = st["drones"].get(d, {})
                cells += [
                    _fmt(s.get("auc", float("nan"))),
                    _fmt(100.0 * s.get("called", float("nan")), 0),
                    s.get("alarmed", "-"),
                ]
            cells += [_fmt(100.0 * st["neg"]["called"], 1), st["neg"]["alarmed"]]
            parts.append("| " + " | ".join(cells) + " |")
            if sq == FIELD_SQUELCHES[-1]:
                for c in clips:
                    g = group_of.get(c.id, c.id)
                    d = c.decisions
                    fired = bool(d.shape[0]) and clip_alarmed(d, thr, rule)
                    cell = (
                        f"{100.0 * (d >= thr).mean():.0f}{'*' if fired else ''}"
                        if d.shape[0]
                        else "-"
                    )
                    per_rec.setdefault(g, {})[m.name] = cell
    parts += [
        "",
        f"### Per recording at squelch {FIELD_SQUELCHES[-1]:g}: windows called drone %, "
        "`*` = the alarm fired",
        "",
    ]
    names = [m.name for m, _ in models]
    labels = dict(zip(rows["group"], rows["label"], strict=False))
    parts += [
        "| recording | label | " + " | ".join(names) + " |",
        "|---|---|" + "---|" * len(names),
    ]
    for g in sorted(per_rec, key=lambda g: (-int(labels.get(g, 0)), g)):
        cells = [per_rec[g].get(n, "-") for n in names]
        short = g.split("/", 1)[1] if "/" in g else g
        parts.append(f"| {short} | {int(labels.get(g, 0))} | " + " | ".join(cells) + " |")
    parts.append("")
    return parts


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
        "`win_tpr` = positive windows called drone; `neg_ok_%` = negative windows correctly left "
        "alone; `fa_per_h` = negative windows called drone per hour of negative audio; "
        f"`clip_det` / `clip_fa` = clips alarmed under the {rule_txt} rule, over the clips long "
        "enough for it (detected/positives, alarmed/negatives).",
        "",
        "The `test` suite is the held-out third of the training sources, grouped so that a "
        "recording never straddles the split. No weight, threshold or model choice is fitted "
        "on it. `val` is where the threshold comes from, so its numbers are optimistic by "
        "construction and are kept only to show by how much.",
        "",
    ]
    tables = []
    details = []
    summary_rows = []
    thresholds: list[tuple[ModelUnderTest, float]] = []
    for m in models:
        df, thr, scored = evaluate_model(manifest, cache, m, rule, fa_per_hour)
        tables.append(df)
        thresholds.append((m, thr))
        by_suite = {r.suite: r for r in df.itertuples(index=False)}

        def cell(suite: str, field: str, nd: int = 3, rows=by_suite) -> str:
            r = rows.get(suite)
            if r is None:
                return "-"
            v = getattr(r, field)
            return v if isinstance(v, str) else _fmt(v, nd)

        def pct(suite: str, field: str, nd: int = 2, rows=by_suite) -> str:
            r = rows.get(suite)
            return "-" if r is None else _fmt(100.0 * getattr(r, field), nd)

        stress_pct = "-"
        if "stress" in scored:
            neg = [c for c in scored["stress"] if c.label == 0]
            tot = sum(c.decisions.shape[0] for c in neg)
            fired = sum(int((c.decisions >= thr).sum()) for c in neg)
            stress_pct = f"{100.0 * fired / tot:.1f}" if tot else "-"
        summary_rows.append(
            {
                "model": m.name,
                "tpr_test_%": pct("test", "win_tpr"),
                "neg_ok_test_%": pct("test", "win_neg_ok"),
                "auc_test": cell("test", "auc"),
                "auc_val": cell("val", "auc"),
                "auc_halmstad": cell("halmstad", "auc"),
                "auc_salford": cell("salford", "auc"),
                "auc_real": cell("real_mic", "auc"),
                "tpr_halmstad": cell("halmstad", "win_tpr", 2),
                "clip_det_halmstad": cell("halmstad", "clip_det"),
                "stress_fired_pct": stress_pct,
                "_sort": by_suite["halmstad"].auc if "halmstad" in by_suite else float("nan"),
                "_sort2": by_suite["test"].auc if "test" in by_suite else float("nan"),
            }
        )
        if "stress" in scored:
            details.append(
                (f"{m.name}: stress probes", per_category_false_alarms(scored["stress"], thr))
            )
        if "val" in scored:
            cat = per_category_false_alarms(scored["val"], thr)
            cat = cat[cat["fired"] > 0].head(8)
            if not cat.empty:
                details.append((f"{m.name}: val negatives that fire", cat))
    if summary_rows:
        summary_rows.sort(key=lambda r: -(r["_sort"] if r["_sort"] == r["_sort"] else -1.0))
        cols = [c for c in summary_rows[0] if not c.startswith("_")]
        parts += [
            "## Summary, ranked by window AUC on Halmstad (unseen)",
            "",
            "| " + " | ".join(cols) + " |",
            "|" + "|".join("---" for _ in cols) + "|",
        ]
        for r in summary_rows:
            parts.append("| " + " | ".join(str(r[c]) for c in cols) + " |")
        parts.append("")
    parts += render_field_section(manifest, cache, thresholds, rule, fa_per_hour)
    parts += ["## Per suite", ""]
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
    from boomdetect_train.run import load_folds, load_run_models, run_info

    models = shipped_models_under_test()
    shipped_names = {m.name for m in models}
    folds = load_folds(run_dir)
    trained_on_field = bool(run_info(run_dir).get("field", {}).get("used", False))
    for name, sm in load_run_models(run_dir).items():
        # A retrained family keeps its registry name (mlp_l2 ...); in a table
        # next to the shipped model of that name it needs telling apart.
        label = f"{name}@{run_dir.name}" if name in shipped_names else name
        m = ModelUnderTest(label, sm.layout, sm.score)
        if folds is not None and all(name in fm for fm in folds[1].values()):
            m.fold_scorers = {k: fm[name].score for k, fm in folds[1].items()}
            m.fold_of_group = folds[0]
        elif trained_on_field:
            m.field_in_sample = True
        models.append(m)
    return render_report(
        f"Comparison: {run_dir.name}", manifest, cache, models, parse_rule(rule), fa_per_hour
    )

"""A training run: every requested family on every requested layout, saved together.

    runs/<name>/run.json            settings, dataset summary, window counts
    runs/<name>/models/<m>.joblib   a ScaledModel (scikit-learn families)
    runs/<name>/models/<m>.npz/json a CnnModel (weights + descriptor)
    runs/<name>/models/<m>.json     kind, layout, offset, width, training meta
    runs/<name>/folds.json          field recording -> fold, when the run has folds
    runs/<name>/folds/<k>/models/   the same models trained without fold k's recordings

Model names are `<family>_l<layout>` for the scikit-learn families and the
architecture name for the CNNs (they only ever read layout 3). The C registry
names are the same strings.

A run can add the node's field recordings to the public training windows
(`field=True`), leave some of them out by category (`field_exclude`, e.g. a
drone), weight the sources by share (train.share_weights) and train the fold
models that judge the field recordings: with `folds=k` every recording falls
in one of k folds and fold j's models never see fold j's recordings. The
exported models are always the ones trained on everything; the fold models
exist only to say how those would do on a recording they have not heard. The
folds are dealt over every field recording in the manifest, included or not,
so two runs that differ only in what they leave out are judged on the same
folds.

Exporting a run writes, per model, the weight header and the translation unit
under models/, and one parity header under tests/vectors/ covering the shipped
models and the exported ones. The registry lines to paste are printed.
"""

from __future__ import annotations

import json
import time
from pathlib import Path

import joblib
import numpy as np
import pandas as pd

from boomdetect_train import export as ex
from boomdetect_train import export_c as exc
from boomdetect_train.augment import variants_of
from boomdetect_train.datasets.cache import CachedFrames, FrameCache, frame_rows
from boomdetect_train.datasets.field import SPLIT as FIELD_SPLIT
from boomdetect_train.datasets.field import assign_folds, load_field_audio
from boomdetect_train.datasets.manifest import ROLE_FIELD, summarize
from boomdetect_train.dsp.audio import to_16k
from boomdetect_train.dsp.mfcc import Frontend
from boomdetect_train.dsp.windows import DEFAULT_SQUELCH
from boomdetect_train.evaluate import clip_windows, score_clips, select_suite, threshold_for_fa_rate
from boomdetect_train.features import LAYOUT_LOGMEL, LAYOUT_STATS, LAYOUT_STATS_SPECTRAL
from boomdetect_train.models.cnn import ARCHS, CnnModel, load_cnn, save_cnn, train_cnn
from boomdetect_train.models.headers import shipped_models
from boomdetect_train.paths import MODELS_DIR, VECTORS_DIR, runs_dir
from boomdetect_train.train import (
    ScaledModel,
    WindowSet,
    build_window_set,
    concat_window_sets,
    frames_window_set,
    share_weights,
    train_gbt,
    train_mlp,
    train_svm,
)

SKLEARN_FAMILIES = {
    "mlp": train_mlp,
    "svm": train_svm,
    "gbt": train_gbt,
    # Regularised variants: the first run showed the plain MLP and forest fitting
    # the training sources and losing on the unseen ones.
    "mlp_reg": lambda ws: train_mlp(ws, hidden=(16,), alpha=1e-2),
    "gbt_reg": lambda ws: train_gbt(ws, max_iter=120, max_leaf_nodes=7, learning_rate=0.05),
}
DEFAULT_FAMILIES = ["mlp", "svm", "gbt", "mlp_reg", "gbt_reg", *sorted(ARCHS)]
DEFAULT_LAYOUTS = [LAYOUT_STATS, LAYOUT_STATS_SPECTRAL, LAYOUT_LOGMEL]
EXPORT_FA_PER_HOUR = 5.0


def _save_meta(run_dir: Path, name: str, meta: dict) -> None:
    (run_dir / "models" / f"{name}.meta.json").write_text(json.dumps(meta, indent=2, default=str))


def field_folds(manifest: pd.DataFrame, k: int) -> dict[str, int]:
    """Fold of every field recording in the manifest, drones and negatives dealt evenly."""
    rows = manifest[manifest["role"] == ROLE_FIELD]
    strata = {
        g: (str(cat) if int(lab) == 1 else "negative")
        for g, cat, lab in rows[["group", "category", "label"]]
        .drop_duplicates("group")
        .itertuples(index=False)
    }
    return assign_folds(strata, k)


def _field_manifest(manifest: pd.DataFrame, exclude: list[str]) -> pd.DataFrame:
    rows = manifest[manifest["role"] == ROLE_FIELD]
    return rows[~rows["category"].isin(exclude)]


def _fit_families(
    ws: WindowSet,
    layout: int,
    families: list[str],
    out_dir: Path,
    base_meta: dict,
    cnn_epochs: int,
    log,
) -> dict[str, dict]:
    """Train every requested family of one layout on `ws`, save them under out_dir."""
    (out_dir / "models").mkdir(parents=True, exist_ok=True)
    metas: dict[str, dict] = {}
    if layout == LAYOUT_LOGMEL:
        for arch in families:
            if arch not in ARCHS:
                continue
            t0 = time.time()
            model = train_cnn(arch, ws.x, ws.y, epochs=cnn_epochs, log=log)
            save_cnn(model, out_dir / "models" / arch)
            meta = {
                "kind": "cnn",
                "layout": model.layout,
                "offset": 0,
                "n_features": model.n_features,
                "seconds": round(time.time() - t0, 1),
                **base_meta,
                **model.meta,
            }
            _save_meta(out_dir, arch, meta)
            metas[arch] = meta
            log(f"  {arch}: trained in {meta['seconds']} s, {meta['macs']} MACs")
        return metas

    for fam in families:
        if fam not in SKLEARN_FAMILIES:
            continue
        t0 = time.time()
        model: ScaledModel = SKLEARN_FAMILIES[fam](ws)
        name = f"{fam}_l{layout}"
        joblib.dump(model, out_dir / "models" / f"{name}.joblib")
        meta = {
            "kind": model.kind,
            "layout": model.layout,
            "offset": model.offset,
            "n_features": model.n_features,
            "seconds": round(time.time() - t0, 1),
            **base_meta,
            **model.meta,
        }
        _save_meta(out_dir, name, meta)
        metas[name] = meta
        log(f"  {name}: trained in {meta['seconds']} s")
    return metas


def _weighted(ws: WindowSet, shares: dict[str, float] | None) -> WindowSet:
    if shares:
        ws.w = share_weights(ws, shares)
    return ws


AUG_SOURCE = "field_aug"


def _augmented_frames(field_rows: pd.DataFrame, count: int, log) -> list[tuple]:
    """(variant id, group, label, CachedFrames) for `count` variants of every field clip."""
    fe = Frontend()
    out = []
    t0 = time.time()
    for rec in field_rows.itertuples(index=False):
        x, sr = load_field_audio(rec.path)
        for vid, y, _ in variants_of(rec.id, x, sr, int(rec.label), count):
            cf = CachedFrames(frame_rows(fe.process(to_16k(y, sr))))
            out.append((vid, rec.group, int(rec.label), cf))
    log(f"{len(out)} augmented field clips in {time.time() - t0:.0f} s")
    return out


def train_all(
    manifest: pd.DataFrame,
    cache: FrameCache,
    *,
    families: list[str] | None = None,
    layouts: list[int] | None = None,
    run_name: str | None = None,
    max_neg_windows_per_clip: int | None = None,
    cnn_epochs: int = 40,
    field: bool = False,
    field_exclude: list[str] | None = None,
    shares: dict[str, float] | None = None,
    folds: int = 0,
    augment: int = 0,
    log=print,
) -> Path:
    families = families or DEFAULT_FAMILIES
    layouts = layouts or DEFAULT_LAYOUTS
    field_exclude = list(field_exclude or [])
    if shares and LAYOUT_LOGMEL in layouts and any(f in ARCHS for f in families):
        raise ValueError("source shares are not implemented for the CNN families")
    if folds and not field:
        raise ValueError("folds split the field recordings; they need field=True")
    if augment and not field:
        raise ValueError("augmentation makes variants of the field recordings; it needs field=True")
    run_dir = runs_dir() / (run_name or time.strftime("%Y%m%d-%H%M%S"))
    (run_dir / "models").mkdir(parents=True, exist_ok=True)

    # A run is often filled in two passes - the cheap stats layouts over every
    # window, then the CNNs over a capped set - so a second call adds to the
    # run rather than forgetting what the first one trained. The models on
    # disk always survived; run.json used to be the thing that lost them.
    info: dict = {"families": [], "layouts": [], "max_neg_windows_per_clip": {}, "models": {}}
    run_json = run_dir / "run.json"
    if run_json.exists():
        info |= json.loads(run_json.read_text())
        info.setdefault("models", {})
    info["dataset"] = summarize(manifest).to_dict(orient="records")
    info["families"] = sorted({*info.get("families", []), *families})
    info["layouts"] = sorted({*info.get("layouts", []), *layouts})
    caps = info.get("max_neg_windows_per_clip")
    caps = dict(caps) if isinstance(caps, dict) else {}
    for lay in layouts:
        caps[str(lay)] = max_neg_windows_per_clip
    info["max_neg_windows_per_clip"] = caps
    info["field"] = {"used": field, "exclude": field_exclude, "folds": folds, "augment": augment}
    info["shares"] = dict(shares or {})

    field_rows = _field_manifest(manifest, field_exclude) if field else manifest.iloc[:0]
    fold_of: dict[str, int] = {}
    if folds:
        fold_of = field_folds(manifest, folds)
        (run_dir / "folds.json").write_text(json.dumps(fold_of, indent=2, sort_keys=True))
        log(f"{len(fold_of)} field recordings in {folds} folds")
    group_of_clip = dict(zip(field_rows["id"], field_rows["group"], strict=True))
    # Variants carry the group of the recording they came from, so a fold that
    # holds a recording out holds its variants out too.
    augmented = _augmented_frames(field_rows, augment, log) if augment else []
    group_of_clip |= {vid: group for vid, group, _, _ in augmented}

    for layout in layouts:
        parts = [
            build_window_set(
                manifest, cache, layout, "train", max_neg_windows_per_clip=max_neg_windows_per_clip
            )
        ]
        if field:
            parts.append(
                build_window_set(field_rows, cache, layout, FIELD_SPLIT, roles=(ROLE_FIELD,))
            )
            log(
                f"layout {layout}: {parts[1].n} field windows "
                f"({int((parts[1].y == 1).sum())} positive) from {field_rows['group'].nunique()} "
                f"recordings, excluding {field_exclude or 'nothing'}"
            )
        if augmented:
            aug = frames_window_set(
                ((vid, AUG_SOURCE, label, cf) for vid, _, label, cf in augmented), layout
            )
            parts.append(aug)
            log(f"layout {layout}: {aug.n} augmented windows ({int((aug.y == 1).sum())} positive)")
        ws = _weighted(concat_window_sets(*parts), shares)
        log(f"layout {layout}: {ws.n} training windows ({int((ws.y == 1).sum())} positive)")
        base_meta = {
            "train_windows": int(ws.n),
            "train_positive": int((ws.y == 1).sum()),
            "field": bool(field),
            "field_exclude": field_exclude,
            "augment": augment,
            "shares": dict(shares or {}),
        }
        info["models"].update(
            _fit_families(ws, layout, families, run_dir, base_meta, cnn_epochs, log)
        )

        for k in range(folds):
            held = np.asarray(
                [fold_of.get(group_of_clip.get(c, ""), -1) == k for c in ws.clip], dtype=bool
            )
            ws_k = _weighted(ws.take(~held), shares)
            log(f"layout {layout}, fold {k}: {int(held.sum())} field windows held out")
            meta_k = {**base_meta, "fold": k, "train_windows": int(ws_k.n)}
            _fit_families(
                ws_k, layout, families, run_dir / "folds" / str(k), meta_k, cnn_epochs, log
            )
    run_json.write_text(json.dumps(info, indent=2, default=str))
    return run_dir


def load_run_models(run_dir: Path) -> dict[str, ScaledModel | CnnModel]:
    out: dict[str, ScaledModel | CnnModel] = {}
    for p in sorted((run_dir / "models").glob("*.joblib")):
        out[p.stem] = joblib.load(p)
    for p in sorted((run_dir / "models").glob("*.npz")):
        out[p.stem] = load_cnn(p)
    return out


def load_folds(run_dir: Path) -> tuple[dict[str, int], dict[int, dict]] | None:
    """(field recording -> fold, fold -> its models) of a run trained with folds, else None."""
    p = run_dir / "folds.json"
    if not p.exists():
        return None
    fold_of = {g: int(k) for g, k in json.loads(p.read_text()).items()}
    models = {
        int(d.name): load_run_models(d)
        for d in sorted((run_dir / "folds").iterdir())
        if d.is_dir() and d.name.isdigit()
    }
    return fold_of, models


def run_info(run_dir: Path) -> dict:
    p = run_dir / "run.json"
    return json.loads(p.read_text()) if p.exists() else {}


def _val_features(manifest: pd.DataFrame, cache: FrameCache, layout: int, n_clips: int = 400):
    """Firmware-policy windows of a slice of the val suite, for parity vectors."""
    rows = select_suite(manifest, "val")
    rows = rows[[cache.has(s) for s in rows["source"]]]
    rows = pd.concat(
        [rows[rows["label"] == 1].head(n_clips // 2), rows[rows["label"] == 0].head(n_clips // 2)]
    )
    feats = []
    for rec in rows.itertuples(index=False):
        f, _ = clip_windows(cache.get(rec.source, rec.id), layout)
        if f.shape[0]:
            feats.append(f)
    return np.concatenate(feats) if feats else np.empty((0, 0), np.float32)


def _val_threshold(
    manifest: pd.DataFrame,
    cache: FrameCache,
    model,
    fa_per_hour: float,
    squelch: float = DEFAULT_SQUELCH,
) -> float:
    """The operating point the exported model ships with: FA budget on the val negatives.

    Chosen under the squelch the board will run with: a lower gate lets quieter
    windows through, and a budget per hour of negative audio is only kept if
    they are counted.
    """
    rows = select_suite(manifest, "val")
    rows = rows[[cache.has(s) for s in rows["source"]]]
    clips = score_clips(rows, cache, model.layout, model.score, squelch=squelch)
    thr = threshold_for_fa_rate(clips, fa_per_hour)
    return 0.0 if np.isnan(thr) else float(thr)


class _HeaderAsModel:
    """Give a header scorer the .score/.offset/.layout shape the exporters expect."""

    def __init__(self, hdr, layout: int):
        self.hdr = hdr
        self.layout = layout
        self.offset = hdr.offset

    def score(self, features):
        return self.hdr.score(features)


def _export_names(models: list[str] | None, run_models: dict) -> list[tuple[str, str]]:
    """(name in the run, name in the C tree) pairs; `mlp_l2=mlp_f1_l2` exports under a new name.

    A retrained family keeps its run name (mlp_l2 ...), which may already be a
    registry entry from an earlier run; the rename is what lets both stay.
    """
    pairs = []
    for spec in models or sorted(run_models):
        src, _, dst = spec.partition("=")
        dst = dst or src
        if src not in run_models:
            raise KeyError(f"{src!r} is not a model of this run: {sorted(run_models)}")
        if not dst.isidentifier():
            raise ValueError(f"{dst!r} is not a C identifier")
        pairs.append((src, dst))
    return pairs


def export_run(
    run_dir: Path,
    manifest: pd.DataFrame,
    cache: FrameCache,
    models: list[str] | None = None,
    fa_per_hour: float = EXPORT_FA_PER_HOUR,
    keep: list[str] | None = None,
    squelch: float = DEFAULT_SQUELCH,
    log=print,
) -> list[Path]:
    """Write each model's header + translation unit, and the parity header for all.

    The parity header has to cover every model in the registry, and the
    registry can hold models from earlier runs: `keep` names them as
    "RUN:MODEL", and they get parity vectors (from their run's weights, which
    are the ones their headers were written from) but no new C files.
    """
    written: list[Path] = []
    run_models = load_run_models(run_dir)
    pairs = _export_names(models, run_models)
    stamp = time.strftime("%Y-%m-%dT%H:%M:%S")
    prov = [f"run: {run_dir.name}", f"generated: {stamp}"]

    entries: list[tuple[str, int, int, ex.ParityVectors]] = []
    feats_by_layout: dict[int, np.ndarray] = {}

    def feats(layout: int) -> np.ndarray:
        if layout not in feats_by_layout:
            feats_by_layout[layout] = _val_features(manifest, cache, layout)
        return feats_by_layout[layout]

    # The shipped models first, scored by their headers; their parity vectors
    # are what proves the harness itself against the known-good C.
    for name, hdr in shipped_models().items():
        wrapper = _HeaderAsModel(hdr, LAYOUT_STATS)
        entries.append(
            (name, LAYOUT_STATS, hdr.offset, ex.parity_vectors(wrapper, feats(LAYOUT_STATS)))
        )

    loaded: dict[str, dict] = {}
    for spec in keep or []:
        run, sep, name = spec.partition(":")
        if not sep:
            raise ValueError(f"--keep wants RUN:MODEL, got {spec!r}")
        if run not in loaded:
            loaded[run] = load_run_models(runs_dir() / run)
        m = loaded[run][name]
        entries.append((name, m.layout, m.offset, ex.parity_vectors(m, feats(m.layout))))
        log(f"{name}: parity vectors from run {run} (already in the C tree)")

    names = [dst for _, dst in pairs]
    for src, name in pairs:
        m = run_models[src]
        thr = _val_threshold(manifest, cache, m, fa_per_hour, squelch)
        thr_milli = exc.thr_milli_from(thr)
        p = prov + [
            f"family: {m.kind}",
            f"layout: {m.layout}",
            f"default threshold: {thr:.3f} (<= {fa_per_hour:g} FA/h on val at squelch {squelch:g})",
        ]
        if src != name:
            p.append(f"trained as: {src}")
        if isinstance(m, CnnModel):
            written.append(
                ex.write_text(MODELS_DIR / f"nn_model_data_{name}.h", exc.cnn_header(m, name, p))
            )
            written.append(
                ex.write_text(MODELS_DIR / f"model_{name}.c", exc.cnn_tu(name, thr_milli, p))
            )
        else:
            header = ex.export_model(m, name, p)
            prefix = {"mlp": "mlp", "svm": "svm", "gbt": "gbt"}[m.kind]
            written.append(ex.write_text(MODELS_DIR / f"{prefix}_model_data_{name}.h", header))
            tu = {"mlp": exc.mlp_tu, "svm": exc.linear_tu, "gbt": exc.gbt_tu}[m.kind]
            written.append(
                ex.write_text(
                    MODELS_DIR / f"model_{name}.c", tu(name, m.layout, m.offset, thr_milli, p)
                )
            )
        entries.append((name, m.layout, m.offset, ex.parity_vectors(m, feats(m.layout))))
        log(f"{name}: threshold {thr:.3f} (<= {fa_per_hour:g} FA/h on val at squelch {squelch:g})")

    written.append(
        ex.write_text(VECTORS_DIR / "parity_vectors.h", ex.export_parity_header(entries, prov))
    )
    decls, regs = exc.registry_lines(names)
    log("\nRegister in models/models.h:\n" + decls)
    log("\nand in src/classifier_registry.c:\n" + regs)
    return written

"""A training run: every requested family on every requested layout, saved together.

    runs/<name>/run.json            settings, dataset summary, window counts
    runs/<name>/models/<m>.joblib   a ScaledModel (scikit-learn families)
    runs/<name>/models/<m>.npz/json a CnnModel (weights + descriptor)
    runs/<name>/models/<m>.json     kind, layout, offset, width, training meta

Model names are `<family>_l<layout>` for the scikit-learn families and the
architecture name for the CNNs (they only ever read layout 3). The C registry
names are the same strings.

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
from boomdetect_train.datasets.cache import FrameCache
from boomdetect_train.datasets.manifest import summarize
from boomdetect_train.evaluate import clip_windows, score_clips, select_suite, threshold_for_fa_rate
from boomdetect_train.features import LAYOUT_LOGMEL, LAYOUT_STATS, LAYOUT_STATS_SPECTRAL
from boomdetect_train.models.cnn import ARCHS, CnnModel, load_cnn, save_cnn, train_cnn
from boomdetect_train.models.headers import shipped_models
from boomdetect_train.paths import MODELS_DIR, VECTORS_DIR, runs_dir
from boomdetect_train.train import (
    ScaledModel,
    WindowSet,
    build_window_set,
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


def train_all(
    manifest: pd.DataFrame,
    cache: FrameCache,
    *,
    families: list[str] | None = None,
    layouts: list[int] | None = None,
    run_name: str | None = None,
    max_neg_windows_per_clip: int | None = None,
    cnn_epochs: int = 40,
    log=print,
) -> Path:
    families = families or DEFAULT_FAMILIES
    layouts = layouts or DEFAULT_LAYOUTS
    run_dir = runs_dir() / (run_name or time.strftime("%Y%m%d-%H%M%S"))
    (run_dir / "models").mkdir(parents=True, exist_ok=True)

    info: dict = {
        "families": families,
        "layouts": layouts,
        "max_neg_windows_per_clip": max_neg_windows_per_clip,
        "dataset": summarize(manifest).to_dict(orient="records"),
        "models": {},
    }
    for layout in layouts:
        ws: WindowSet = build_window_set(
            manifest, cache, layout, "train", max_neg_windows_per_clip=max_neg_windows_per_clip
        )
        log(f"layout {layout}: {ws.n} training windows ({int((ws.y == 1).sum())} positive)")
        base_meta = {"train_windows": int(ws.n), "train_positive": int((ws.y == 1).sum())}

        if layout == LAYOUT_LOGMEL:
            for arch in families:
                if arch not in ARCHS:
                    continue
                t0 = time.time()
                model = train_cnn(arch, ws.x, ws.y, epochs=cnn_epochs, log=log)
                save_cnn(model, run_dir / "models" / arch)
                meta = {
                    "kind": "cnn",
                    "layout": model.layout,
                    "offset": 0,
                    "n_features": model.n_features,
                    "seconds": round(time.time() - t0, 1),
                    **base_meta,
                    **model.meta,
                }
                _save_meta(run_dir, arch, meta)
                info["models"][arch] = meta
                log(f"  {arch}: trained in {meta['seconds']} s, {meta['macs']} MACs")
            continue

        for fam in families:
            if fam not in SKLEARN_FAMILIES:
                continue
            t0 = time.time()
            model: ScaledModel = SKLEARN_FAMILIES[fam](ws)
            name = f"{fam}_l{layout}"
            joblib.dump(model, run_dir / "models" / f"{name}.joblib")
            meta = {
                "kind": model.kind,
                "layout": model.layout,
                "offset": model.offset,
                "n_features": model.n_features,
                "seconds": round(time.time() - t0, 1),
                **base_meta,
                **model.meta,
            }
            _save_meta(run_dir, name, meta)
            info["models"][name] = meta
            log(f"  {name}: trained in {meta['seconds']} s")
    (run_dir / "run.json").write_text(json.dumps(info, indent=2, default=str))
    return run_dir


def load_run_models(run_dir: Path) -> dict[str, ScaledModel | CnnModel]:
    out: dict[str, ScaledModel | CnnModel] = {}
    for p in sorted((run_dir / "models").glob("*.joblib")):
        out[p.stem] = joblib.load(p)
    for p in sorted((run_dir / "models").glob("*.npz")):
        out[p.stem] = load_cnn(p)
    return out


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


def _val_threshold(manifest: pd.DataFrame, cache: FrameCache, model, fa_per_hour: float) -> float:
    """The operating point the exported model ships with: FA budget on the val negatives."""
    rows = select_suite(manifest, "val")
    rows = rows[[cache.has(s) for s in rows["source"]]]
    clips = score_clips(rows, cache, model.layout, model.score)
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


def export_run(
    run_dir: Path,
    manifest: pd.DataFrame,
    cache: FrameCache,
    models: list[str] | None = None,
    fa_per_hour: float = EXPORT_FA_PER_HOUR,
    log=print,
) -> list[Path]:
    """Write each model's header + translation unit, and the parity header for all."""
    written: list[Path] = []
    run_models = load_run_models(run_dir)
    names = models or sorted(run_models)
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

    for name in names:
        m = run_models[name]
        thr = _val_threshold(manifest, cache, m, fa_per_hour)
        thr_milli = exc.thr_milli_from(thr)
        p = prov + [f"family: {m.kind}", f"layout: {m.layout}", f"default threshold: {thr:.3f}"]
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
        log(f"{name}: threshold {thr:.3f} (<= {fa_per_hour:g} FA/h on val)")

    written.append(
        ex.write_text(VECTORS_DIR / "parity_vectors.h", ex.export_parity_header(entries, prov))
    )
    decls, regs = exc.registry_lines(names)
    log("\nRegister in models/models.h:\n" + decls)
    log("\nand in src/classifier_registry.c:\n" + regs)
    return written

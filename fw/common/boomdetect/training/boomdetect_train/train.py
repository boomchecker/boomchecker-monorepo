"""Training rows from the frame cache, and the scikit-learn model families.

Rows are windows, not clips: the board classifies 14-frame windows, so a model
trained on whole-clip statistics meets a different distribution at run time
(the E0-versus-E1 finding on the research branch). Training windows slide
with a hop of 7 frames over each clip - twice the windows of the disjoint
firmware policy, and the overlap is harmless for a classifier - while every
evaluation uses the firmware policy through evaluate.py.

Positive windows quieter than POS_WIN_MIN_RMS (median frame RMS) are dropped:
a drone clip's silent tail is label noise. Negatives keep every window; a
quiet negative is still a negative.

Feature 0 of layouts 1 and 2 (the mean of MFCC coefficient 0) is the only
level-dependent value and is never fed to a new model: the models are meant to
be gain-invariant, which is also why the layout-3 patch has its mean removed.
"""

from __future__ import annotations

from dataclasses import dataclass, field

import numpy as np
import pandas as pd
from sklearn.ensemble import HistGradientBoostingClassifier
from sklearn.neural_network import MLPClassifier
from sklearn.preprocessing import StandardScaler
from sklearn.svm import LinearSVC

from boomdetect_train.datasets.cache import FrameCache
from boomdetect_train.datasets.manifest import ROLE_TRAIN
from boomdetect_train.dsp.windows import Gate
from boomdetect_train.evaluate import clip_windows
from boomdetect_train.features import LAYOUT_LOGMEL, LAYOUTS

POS_WIN_MIN_RMS = 0.002
TRAIN_HOP = 7
SEED = 42


def feature_offset(layout: int) -> int:
    """First feature a new model reads: layouts 1 and 2 skip the level-carrying mean-c0."""
    return 0 if layout == LAYOUT_LOGMEL else 1


@dataclass
class WindowSet:
    """Feature rows with their labels and the clip each came from."""

    x: np.ndarray  # (n, n_features_full) - the FULL layout; models slice it
    y: np.ndarray  # (n,) int
    clip: np.ndarray  # (n,) str clip ids, for grouping
    layout: int

    @property
    def n(self) -> int:
        return int(self.y.shape[0])


def build_window_set(
    manifest: pd.DataFrame,
    cache: FrameCache,
    layout: int,
    split: str,
    *,
    hop: int = TRAIN_HOP,
    pos_min_rms: float = POS_WIN_MIN_RMS,
    max_neg_windows_per_clip: int | None = None,
    seed: int = SEED,
) -> WindowSet:
    """Sliding training windows for every train-role clip of `split`."""
    rows = manifest[(manifest["role"] == ROLE_TRAIN) & (manifest["split"] == split)]
    rng = np.random.default_rng(seed)
    xs, ys, ids = [], [], []
    for rec in rows.itertuples(index=False):
        cf = cache.get(rec.source, rec.id)
        squelch = pos_min_rms if rec.label == 1 else None
        feats, _ = clip_windows(cf, layout, Gate.WINDOW_MEDIAN, squelch, hop=hop)
        if feats.shape[0] == 0:
            continue
        if (
            rec.label == 0
            and max_neg_windows_per_clip
            and feats.shape[0] > max_neg_windows_per_clip
        ):
            pick = rng.choice(feats.shape[0], size=max_neg_windows_per_clip, replace=False)
            feats = feats[np.sort(pick)]
        xs.append(feats)
        ys.append(np.full(feats.shape[0], int(rec.label), dtype=np.int64))
        ids.extend([rec.id] * feats.shape[0])
    n_feat = LAYOUTS[layout].n_features
    if not xs:
        return WindowSet(
            np.empty((0, n_feat), np.float32), np.empty(0, np.int64), np.empty(0, str), layout
        )
    return WindowSet(
        np.concatenate(xs).astype(np.float32),
        np.concatenate(ys),
        np.asarray(ids, dtype=str),
        layout,
    )


def balance_by_oversampling(ws: WindowSet, seed: int = SEED) -> tuple[np.ndarray, np.ndarray]:
    """Oversample the minority class to parity, then shuffle (what the v6 recipe did)."""
    rng = np.random.default_rng(seed)
    pos = np.flatnonzero(ws.y == 1)
    neg = np.flatnonzero(ws.y == 0)
    if pos.size == 0 or neg.size == 0:
        raise ValueError("training set needs both classes")
    minority, majority = (pos, neg) if pos.size < neg.size else (neg, pos)
    extra = rng.choice(minority, size=majority.size - minority.size, replace=True)
    idx = rng.permutation(np.concatenate([np.arange(ws.n), extra]))
    return ws.x[idx], ws.y[idx]


# --- the scikit-learn families ----------------------------------------------


@dataclass
class ScaledModel:
    """A StandardScaler in front of a model, the way every shipped model is built."""

    kind: str  # "mlp" | "svm" | "gbt"
    layout: int
    offset: int
    scaler_mean: np.ndarray | None
    scaler_inv_std: np.ndarray | None
    model: object
    meta: dict = field(default_factory=dict)

    @property
    def n_features(self) -> int:
        return int(LAYOUTS[self.layout].n_features - self.offset)

    def _slice(self, features: np.ndarray) -> np.ndarray:
        x = np.atleast_2d(np.asarray(features, dtype=np.float32))
        return x[:, self.offset : self.offset + self.n_features]

    def _scaled(self, features: np.ndarray) -> np.ndarray:
        x = self._slice(features)
        if self.scaler_mean is None:
            return x
        return (x - self.scaler_mean) * self.scaler_inv_std

    def score(self, features: np.ndarray) -> np.ndarray:
        """The decision the C will compute: a logit (mlp, gbt) or a margin (svm)."""
        z = self._scaled(features)
        if self.kind == "mlp":
            out = mlp_logit(self.model, z)
        elif self.kind == "svm":
            out = self.model.decision_function(z.astype(np.float64))
        elif self.kind == "gbt":
            out = self.model.decision_function(z.astype(np.float64))
        else:
            raise ValueError(self.kind)
        out = np.asarray(out, dtype=np.float32).reshape(-1)
        return out if np.ndim(features) > 1 else np.float32(out[0])


def mlp_logit(clf: MLPClassifier, z: np.ndarray) -> np.ndarray:
    """The pre-sigmoid output of a binary MLPClassifier, in float32 like the C."""
    h = z.astype(np.float32)
    for w, b in zip(clf.coefs_[:-1], clf.intercepts_[:-1], strict=True):
        h = np.maximum(h @ w.astype(np.float32) + b.astype(np.float32), np.float32(0))
    return (
        h @ clf.coefs_[-1].astype(np.float32)[:, 0] + np.float32(clf.intercepts_[-1][0])
    ).astype(np.float32)


def _fit_scaler(x: np.ndarray) -> tuple[StandardScaler, np.ndarray, np.ndarray]:
    scaler = StandardScaler().fit(x)
    mean = scaler.mean_.astype(np.float32)
    inv = (1.0 / (scaler.scale_ + 1e-8)).astype(np.float32)
    return scaler, mean, inv


def train_mlp(
    ws: WindowSet, hidden: tuple[int, ...] = (32,), seed: int = SEED, alpha: float = 1e-4
) -> ScaledModel:
    off = feature_offset(ws.layout)
    x, y = balance_by_oversampling(ws, seed)
    x = x[:, off:]
    scaler, mean, inv = _fit_scaler(x)
    clf = MLPClassifier(
        hidden_layer_sizes=hidden,
        activation="relu",
        alpha=alpha,
        batch_size=512,
        learning_rate_init=1e-3,
        max_iter=300,
        early_stopping=True,
        n_iter_no_change=15,
        validation_fraction=0.1,
        random_state=seed,
    )
    clf.fit((x - mean) * inv, y)
    return ScaledModel("mlp", ws.layout, off, mean, inv, clf, {"hidden": list(hidden)})


def train_svm(ws: WindowSet, c: float = 1.0, seed: int = SEED) -> ScaledModel:
    off = feature_offset(ws.layout)
    x = ws.x[:, off:]
    scaler, mean, inv = _fit_scaler(x)
    clf = LinearSVC(C=c, class_weight="balanced", max_iter=20000, random_state=seed)
    clf.fit((x - mean) * inv, ws.y)
    return ScaledModel("svm", ws.layout, off, mean, inv, clf, {"C": c})


def train_gbt(
    ws: WindowSet,
    *,
    max_iter: int = 200,
    max_leaf_nodes: int = 15,
    learning_rate: float = 0.1,
    seed: int = SEED,
) -> ScaledModel:
    """Gradient-boosted trees. No scaler: trees are invariant to monotone rescaling."""
    off = feature_offset(ws.layout)
    x = ws.x[:, off:].astype(np.float64)
    n_pos, n_neg = int((ws.y == 1).sum()), int((ws.y == 0).sum())
    w = np.where(ws.y == 1, n_neg / max(n_pos, 1), 1.0)
    clf = HistGradientBoostingClassifier(
        max_iter=max_iter,
        max_leaf_nodes=max_leaf_nodes,
        learning_rate=learning_rate,
        early_stopping=True,
        validation_fraction=0.1,
        n_iter_no_change=20,
        random_state=seed,
    )
    clf.fit(x, ws.y, sample_weight=w)
    return ScaledModel(
        "gbt",
        ws.layout,
        off,
        None,
        None,
        clf,
        {"max_iter": max_iter, "max_leaf_nodes": max_leaf_nodes, "learning_rate": learning_rate},
    )

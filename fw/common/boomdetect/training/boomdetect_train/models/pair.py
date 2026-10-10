"""Two models on one window, an alarm from either (OR), the way src/classifier_pair.c runs it.

The pairs were picked by analysis/2026-10-07-gs-cnn/combo_eval.py in the data root. Each
member's score goes onto its own false-alarm scale on the val negatives,

    z = -log10((n_ge + 1) / (N + 1)),  n_ge = val-negative windows scoring >= s, N = all,

and the pair decides with the larger z, so its one threshold spends one budget for both.
A ZTable keeps a member's top K negative scores: exact among them, held at z(K) below,
continued along the top decade's slope above (so a threshold can be set stricter than
anything val holds). The pair's layout holds both members' inputs: member A's layout 4
at `base_a`, the network's at `base_b`.
"""

from __future__ import annotations

from dataclasses import dataclass, field

import numpy as np

from boomdetect_train.features import (
    HYBRID_AUX_LAYOUT,
    LAYOUT_STATS_SPECTRAL_MOD,
    LAYOUTS,
    N_STATS_SPECTRAL_MOD,
    SPEC_FE_NAMES,
    SPEC_KINDS,
    spec_layout,
)

ZTABLE_K = 256
SLOPE_SPAN = 10  # z above the top negative continues along top[0] .. top[SLOPE_SPAN]


@dataclass
class ZTable:
    """A member's score -> z on the val negatives, truncated to the top K."""

    top: np.ndarray  # (K,) float32, descending
    n_neg: int
    gain: float  # z per score unit above top[0] (float32), 0 when flat

    @classmethod
    def build(cls, neg_scores: np.ndarray, k: int = ZTABLE_K) -> ZTable:
        s = np.sort(np.asarray(neg_scores, dtype=np.float32))[::-1]
        if s.size == 0:
            raise ValueError("no val-negative scores to build a z table from")
        zt = cls(top=np.ascontiguousarray(s[:k]), n_neg=int(s.size), gain=0.0)
        j = min(SLOPE_SPAN, zt.k - 1)
        span = float(zt.top[0]) - float(zt.top[j])
        if j > 0 and span > 0:
            zt.gain = float(np.float32((float(zt.z_of(0)) - float(zt.z_of(j))) / span))
        return zt

    @property
    def k(self) -> int:
        return int(self.top.shape[0])

    def z_of(self, c) -> np.ndarray:
        """z of c negatives at or above a score: in float64, then float32, as the C does."""
        c = np.asarray(c, dtype=np.float64)
        return (-np.log10((c + 1.0) / (self.n_neg + 1.0))).astype(np.float32)

    def __call__(self, scores: np.ndarray) -> np.ndarray:
        s = np.asarray(scores, dtype=np.float32).reshape(-1)
        c = self.k - np.searchsorted(self.top[::-1], s, side="left")  # top scores >= s
        out = self.z_of(c)
        if self.gain > 0.0:
            above = c == 0
            out[above] = self.z_of(0) + (s[above] - self.top[0]) * np.float32(self.gain)
        out[np.isnan(s)] = -np.inf
        return out


def pair_layout(net_layout: int, a_layout: int) -> tuple[int, int, int]:
    """(pair layout, base of member A, base of member B) for a network and a layout-4 member.

    A hybrid network's own layout already ends in its aux layout, whose first 79 numbers
    are layout 4; a network on the bare patch is paired on the patch-plus-layout-4 hybrid
    of the same length and front-end.
    """
    if a_layout != LAYOUT_STATS_SPECTRAL_MOD:
        raise ValueError(f"member A must be a layout-4 model, not layout {a_layout}")
    spec = spec_layout(net_layout)
    if spec is None:
        raise ValueError(f"layout {net_layout} is not a spectrogram layout")
    kind, fe, t = spec
    patch = t * 64
    if kind in HYBRID_AUX_LAYOUT:
        aux = HYBRID_AUX_LAYOUT[kind]
        if LAYOUTS[aux].n_features < N_STATS_SPECTRAL_MOD:
            raise ValueError(f"aux layout {aux} does not hold layout 4")
        return net_layout, patch, 0
    if kind != "patch":
        raise ValueError(f"layout {net_layout} ({kind}) cannot be paired")
    base = next(b for b, (k, tt) in SPEC_KINDS.items() if k == "hybrid" and tt == t)
    return base + SPEC_FE_NAMES.index(fe), patch, 0


@dataclass
class PairModel:
    """The OR of a cheap member A (a ScaledModel on layout 4) and a network B."""

    kind: str  # "pair"
    name: str
    layout: int  # the pair's layout: both members' inputs in one vector
    a: object  # ScaledModel
    b: object  # TorchNetModel
    a_name: str  # the members' names in the C tree
    b_name: str
    base_a: int
    base_b: int
    za: ZTable
    zb: ZTable
    offset: int = 0
    meta: dict = field(default_factory=dict)

    @property
    def n_features(self) -> int:
        return LAYOUTS[self.layout].n_features

    def _slices(self, x: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
        wa = LAYOUTS[self.a.layout].n_features
        wb = LAYOUTS[self.b.layout].n_features
        return x[:, self.base_a : self.base_a + wa], x[:, self.base_b : self.base_b + wb]

    def member_scores(self, features: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
        x = np.atleast_2d(np.asarray(features, dtype=np.float32))
        xa, xb = self._slices(x)
        sa = np.asarray(self.a.score(xa), dtype=np.float32).reshape(-1)
        sb = np.asarray(self.b.score(xb), dtype=np.float32).reshape(-1)
        return sa, sb

    def score(self, features: np.ndarray) -> np.ndarray:
        sa, sb = self.member_scores(features)
        out = np.maximum(self.za(sa), self.zb(sb)).astype(np.float32)
        return out if np.ndim(features) > 1 else np.float32(out[0])


def build_pair(name, a, a_name, b, b_name, val_neg_x: np.ndarray) -> PairModel:
    """A PairModel whose z tables come from `val_neg_x`, the val-negative windows in the pair's
    layout (the windows a threshold is chosen on)."""
    layout, base_a, base_b = pair_layout(b.layout, a.layout)
    if val_neg_x.shape[1] != LAYOUTS[layout].n_features:
        raise ValueError(f"val windows are {val_neg_x.shape[1]} wide, layout {layout} is not")
    proto = PairModel("pair", name, layout, a, b, a_name, b_name, base_a, base_b, None, None)
    sa, sb = proto.member_scores(val_neg_x)
    proto.za = ZTable.build(sa)
    proto.zb = ZTable.build(sb)
    proto.meta = {
        "members": [a_name, b_name],
        "n_val_neg_windows": int(val_neg_x.shape[0]),
        "macs": (getattr(b, "meta", {}) or {}).get("macs"),
    }
    return proto

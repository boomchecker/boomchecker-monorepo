"""Read the shipped model headers back into Python scorers.

The deployed models exist only as C arrays (models/mlp_model_data_v6.h,
models/svm_model_data_v3.h): the sklearn pickles they came from live on a
research branch and are not needed here. Parsing the headers gives the exact
weights the board multiplies by, which is what a baseline evaluation and a
parity fixture both want.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

import numpy as np

from boomdetect_train.dsp.tables import parse_arrays, parse_defines
from boomdetect_train.paths import MODELS_DIR


@dataclass(frozen=True)
class MlpHeader:
    """A scaler followed by one hidden ReLU layer and a linear output (a logit)."""

    name: str
    offset: int  # first feature read (the MLPs skip feature 0)
    scaler_mean: np.ndarray
    scaler_inv_std: np.ndarray
    w1: np.ndarray  # (hidden, inputs)
    b1: np.ndarray  # (hidden,)
    w2: np.ndarray  # (hidden,)
    b2: float

    @property
    def n_features(self) -> int:
        return int(self.w1.shape[1])

    def score(self, features: np.ndarray) -> np.ndarray:
        """Decision for a (52,) vector or a (n, 52) matrix of full-layout features."""
        x = np.atleast_2d(np.asarray(features, dtype=np.float32))
        x = x[:, self.offset : self.offset + self.n_features]
        z = (x - self.scaler_mean) * self.scaler_inv_std
        h = np.maximum(z @ self.w1.T + self.b1, np.float32(0.0))
        out = h @ self.w2 + np.float32(self.b2)
        return out.astype(np.float32) if np.ndim(features) > 1 else np.float32(out[0])


@dataclass(frozen=True)
class LinearHeader:
    """A scaler followed by a dot product: the linear SVM family."""

    name: str
    offset: int
    scaler_mean: np.ndarray
    scaler_inv_std: np.ndarray
    weights: np.ndarray
    bias: float

    @property
    def n_features(self) -> int:
        return int(self.weights.shape[0])

    def score(self, features: np.ndarray) -> np.ndarray:
        x = np.atleast_2d(np.asarray(features, dtype=np.float32))
        x = x[:, self.offset : self.offset + self.n_features]
        z = (x - self.scaler_mean) * self.scaler_inv_std
        out = z @ self.weights + np.float32(self.bias)
        return out.astype(np.float32) if np.ndim(features) > 1 else np.float32(out[0])


def _float_define(text: str, name: str) -> float:
    import re

    m = re.search(rf"^\s*#define\s+{name}\s+([-+0-9.eE]+)f?\b", text, re.MULTILINE)
    if not m:
        raise ValueError(f"{name} not found")
    return float(m.group(1))


def load_mlp_header(path: Path | str, name: str, offset: int = 1) -> MlpHeader:
    text = Path(path).read_text(encoding="utf-8")
    defines = parse_defines(text)
    arrays = parse_arrays(text, defines)
    return MlpHeader(
        name=name,
        offset=offset,
        scaler_mean=arrays["mlp_scaler_mean"],
        scaler_inv_std=arrays["mlp_scaler_inv_std"],
        w1=arrays["mlp_w1"],
        b1=arrays["mlp_b1"],
        w2=arrays["mlp_w2"],
        b2=_float_define(text, "MLP_B2"),
    )


def load_svm_header(path: Path | str, name: str, offset: int = 0) -> LinearHeader:
    text = Path(path).read_text(encoding="utf-8")
    defines = parse_defines(text)
    arrays = parse_arrays(text, defines)
    return LinearHeader(
        name=name,
        offset=offset,
        scaler_mean=arrays["svm_scaler_mean"],
        scaler_inv_std=arrays["svm_scaler_inv_std"],
        weights=arrays["svm_weights"],
        bias=_float_define(text, "SVM_BIAS"),
    )


def shipped_models() -> dict[str, MlpHeader | LinearHeader]:
    """The two models in the firmware's registry, by their registry names."""
    return {
        "mlp_v6": load_mlp_header(MODELS_DIR / "mlp_model_data_v6.h", "mlp_v6", offset=1),
        "svm_v3": load_svm_header(MODELS_DIR / "svm_model_data_v3.h", "svm_v3", offset=0),
    }


# The operating points the registry declares (classifier_t::default_thr_milli).
SHIPPED_THRESHOLDS = {"mlp_v6": 15.0, "svm_v3": 0.5}

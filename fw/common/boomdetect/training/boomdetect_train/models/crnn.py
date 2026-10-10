"""The band-spectrogram CRNN (models/torchnets.py's crnn2d) in the form src/crnn.c reads.

Batch norm becomes a per-channel scale and shift after the ReLU, the two LSTM biases are
summed. The C is held to the PyTorch module itself by the parity vectors (export.py).
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np

from boomdetect_train.models.torchnets import TorchNetModel, aux_width, build_module, layout_shape

KERNEL = 3


@dataclass
class CrnnParams:
    layout: int
    t: int  # patch frames
    bands: int
    c1: int
    c2: int
    hidden: int
    n_aux: int  # aux numbers the network reads (0 for a plain patch layout)
    aux_hidden: int
    head: int
    patch_std: float
    w1: np.ndarray  # (c1, 1, 3, 3)
    b1: np.ndarray
    s1: np.ndarray  # batch norm after the ReLU: y = s * x + t
    t1: np.ndarray
    w2: np.ndarray  # (c2, c1, 3, 3)
    b2: np.ndarray
    s2: np.ndarray
    t2: np.ndarray
    w_ih: np.ndarray  # (4H, c2 * bands / 4), gates i f g o
    w_hh: np.ndarray  # (4H, H)
    b_gates: np.ndarray  # b_ih + b_hh
    w_head: np.ndarray  # (head, hidden + aux_hidden)
    b_head: np.ndarray
    w_out: np.ndarray  # (head,)
    b_out: float
    aux_fill: np.ndarray | None = None
    aux_mean: np.ndarray | None = None
    aux_std: np.ndarray | None = None
    w_aux: np.ndarray | None = None  # (aux_hidden, n_aux)
    b_aux: np.ndarray | None = None

    @property
    def aux_offset(self) -> int:
        """First aux number in the feature vector: the aux layout's mean-c0 is skipped."""
        return self.t * self.bands + 1

    def macs(self) -> int:
        """Multiply-accumulates per window as the C runs them (rows a pool drops are skipped)."""
        h1, h2 = self.t // 2, self.t // 4
        conv1 = 2 * h1 * self.bands * self.c1 * KERNEL * KERNEL
        conv2 = 2 * h2 * (self.bands // 2) * self.c2 * self.c1 * KERNEL * KERNEL
        lstm = h2 * 4 * self.hidden * (self.c2 * (self.bands // 4) + self.hidden)
        head = self.head * (self.hidden + self.aux_hidden + 1)
        return conv1 + conv2 + lstm + self.aux_hidden * self.n_aux + head

    def n_params(self) -> int:
        arrays = [self.w1, self.b1, self.w2, self.b2, self.w_ih, self.w_hh, self.b_gates]
        arrays += [self.w_head, self.b_head, self.w_out]
        if self.n_aux:
            arrays += [self.w_aux, self.b_aux]
        return int(sum(a.size for a in arrays)) + 1


def crnn_params(model: TorchNetModel) -> CrnnParams:
    """The weights of a trained crnn2d in the C's form."""
    if model.name != "crnn2d":
        raise ValueError(f"only crnn2d has a C form, not {model.name!r}")
    t, b, hybrid = layout_shape(model.layout)
    n_aux = aux_width(model.layout)
    st = {k: v.detach().cpu().numpy() for k, v in model.state.items()}
    module = build_module(model.name, t, b, n_aux)

    def f32(a) -> np.ndarray:
        return np.ascontiguousarray(np.asarray(a, dtype=np.float32))

    def bn(prefix: str, eps: float) -> tuple[np.ndarray, np.ndarray]:
        gamma = st[prefix + ".weight"].astype(np.float64)
        beta = st[prefix + ".bias"].astype(np.float64)
        mean = st[prefix + ".running_mean"].astype(np.float64)
        var = st[prefix + ".running_var"].astype(np.float64)
        scale = gamma / np.sqrt(var + eps)
        return f32(scale), f32(beta - mean * scale)

    s1, t1 = bn("body.2", float(module.body[2].eps))
    s2, t2 = bn("body.6", float(module.body[6].eps))
    p = CrnnParams(
        layout=model.layout,
        t=t,
        bands=b,
        c1=int(st["body.0.weight"].shape[0]),
        c2=int(st["body.4.weight"].shape[0]),
        hidden=int(st["rnn.weight_hh_l0"].shape[1]),
        n_aux=n_aux,
        aux_hidden=int(st["aux.0.weight"].shape[0]) if hybrid else 0,
        head=int(st["head.0.weight"].shape[0]),
        patch_std=float(np.float32(model.norm["patch_std"])),
        w1=f32(st["body.0.weight"]),
        b1=f32(st["body.0.bias"]),
        s1=s1,
        t1=t1,
        w2=f32(st["body.4.weight"]),
        b2=f32(st["body.4.bias"]),
        s2=s2,
        t2=t2,
        w_ih=f32(st["rnn.weight_ih_l0"]),
        w_hh=f32(st["rnn.weight_hh_l0"]),
        b_gates=f32(
            st["rnn.bias_ih_l0"].astype(np.float32) + st["rnn.bias_hh_l0"].astype(np.float32)
        ),
        w_head=f32(st["head.0.weight"]),
        b_head=f32(st["head.0.bias"]),
        w_out=f32(st["head.3.weight"].reshape(-1)),
        b_out=float(np.float32(st["head.3.bias"].reshape(-1)[0])),
    )
    if hybrid:
        p.aux_fill = f32(model.norm["aux_fill"])
        p.aux_mean = f32(model.norm["aux_mean"])
        p.aux_std = f32(model.norm["aux_std"])
        p.w_aux = f32(st["aux.0.weight"])
        p.b_aux = f32(st["aux.0.bias"])
    if p.w_ih.shape != (4 * p.hidden, p.c2 * (b // 4)):
        raise ValueError(f"LSTM input {p.w_ih.shape} does not match the pooled map")
    if p.w_head.shape[1] != p.hidden + p.aux_hidden:
        raise ValueError("head input width does not match hidden + aux")
    return p

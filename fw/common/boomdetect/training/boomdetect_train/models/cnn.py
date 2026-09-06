"""Small convolutional nets over the log-mel patch (layout 3), and their C form.

Every architecture is a list of layer specs. The same list builds the PyTorch
module for training, the numpy reference that reproduces the C interpreter's
arithmetic, and the descriptor table the C reads - so the three cannot describe
different networks. The C side is src/nn_infer.c: plain float32 conv2d
(optionally depthwise), 2x2 max-pool, global average pool and dense layers,
which is all these nets use. No batch norm, so nothing has to be folded.

Sizes are chosen for the board: a few hundred thousand multiply-accumulates per
window at most (a Cortex-M33 at 250 MHz does that in single-digit
milliseconds in float), a few tens of kilobytes of weights.

Input: the 280-value patch is 14 frames x 20 mel bands, frame-major. The 2-D
nets see it as one channel of 14 x 20; the 1-D net sees the 20 mel bands as
channels along 14 time steps (a kernel of 1 x 3 over a 1 x 14 image).
"""

from __future__ import annotations

import json
from dataclasses import asdict, dataclass, field
from pathlib import Path

import numpy as np

from boomdetect_train.dsp.mfcc import N_MELS
from boomdetect_train.dsp.windows import ACCUM_FRAMES
from boomdetect_train.features import LAYOUT_LOGMEL, N_LOGMEL_PATCH

# Layer kinds, shared with src/nn_infer.h by value.
L_CONV = 1  # full conv2d, kernel (kh, kw), 'same' padding, stride 1
L_DWCONV = 2  # depthwise conv2d
L_POOL = 3  # 2x2 max pool, stride 2, floor
L_GAP = 4  # global average pool -> (C, 1, 1)
L_DENSE = 5  # fully connected over the flattened activation

# How the 280-value patch is laid into the input tensor.
IN_FRAME_MAJOR = 0  # (1, 14, 20): in[0][t][m] = patch[t*20 + m]
IN_MEL_CHANNELS = 1  # (20, 1, 14): in[m][0][t] = patch[t*20 + m]

ARCHS: dict[str, tuple[int, list[tuple]]] = {
    # name: (input layout, layers). ("conv", out_c, (kh, kw)) / ("dw", (kh, kw)) /
    # ("pool",) / ("gap",) / ("dense", out). Every conv and every hidden dense
    # is followed by ReLU; the last dense is the logit.
    "cnn_small": (
        IN_FRAME_MAJOR,
        [("conv", 8, (3, 3)), ("pool",), ("conv", 16, (3, 3)), ("gap",), ("dense", 1)],
    ),
    "cnn_ds": (
        IN_FRAME_MAJOR,
        [
            ("conv", 16, (3, 3)),
            ("pool",),
            ("dw", (3, 3)),
            ("conv", 32, (1, 1)),
            ("dw", (3, 3)),
            ("conv", 32, (1, 1)),
            ("gap",),
            ("dense", 1),
        ],
    ),
    "cnn_1d": (
        IN_MEL_CHANNELS,
        [("conv", 32, (1, 3)), ("conv", 32, (1, 3)), ("gap",), ("dense", 1)],
    ),
    "cnn_wide": (
        IN_FRAME_MAJOR,
        [
            ("conv", 16, (3, 3)),
            ("pool",),
            ("conv", 32, (3, 3)),
            ("pool",),
            ("dense", 32),
            ("dense", 1),
        ],
    ),
}


def input_shape(layout: int) -> tuple[int, int, int]:
    return (1, ACCUM_FRAMES, N_MELS) if layout == IN_FRAME_MAJOR else (N_MELS, 1, ACCUM_FRAMES)


def patch_to_input(patch: np.ndarray, layout: int) -> np.ndarray:
    """(n, 280) patches -> (n, C, H, W) tensors for the given input layout."""
    p = np.atleast_2d(np.asarray(patch, dtype=np.float32))
    n = p.shape[0]
    frames = p.reshape(n, ACCUM_FRAMES, N_MELS)
    if layout == IN_FRAME_MAJOR:
        return frames[:, None, :, :]
    return np.transpose(frames, (0, 2, 1))[:, :, None, :]


@dataclass
class Layer:
    kind: int
    relu: bool
    in_shape: tuple[int, int, int]
    out_shape: tuple[int, int, int]
    kernel: tuple[int, int] = (0, 0)
    pad: tuple[int, int] = (0, 0)
    weight: np.ndarray | None = None  # conv: (out_c, in_c_per_group, kh, kw); dense: (out, in)
    bias: np.ndarray | None = None

    @property
    def macs(self) -> int:
        c, h, w = self.out_shape
        if self.kind == L_CONV:
            return c * h * w * self.in_shape[0] * self.kernel[0] * self.kernel[1]
        if self.kind == L_DWCONV:
            return c * h * w * self.kernel[0] * self.kernel[1]
        if self.kind == L_DENSE:
            return int(np.prod(self.in_shape)) * c
        return 0

    @property
    def params(self) -> int:
        return (self.weight.size if self.weight is not None else 0) + (
            self.bias.size if self.bias is not None else 0
        )


@dataclass
class NetSpec:
    """A built network: shapes resolved, weights attached."""

    name: str
    input_layout: int
    layers: list[Layer] = field(default_factory=list)

    @property
    def macs(self) -> int:
        return sum(layer.macs for layer in self.layers)

    @property
    def params(self) -> int:
        return sum(layer.params for layer in self.layers)

    @property
    def max_activation(self) -> int:
        sizes = [int(np.prod(input_shape(self.input_layout)))]
        sizes += [int(np.prod(layer.out_shape)) for layer in self.layers]
        return max(sizes)


def resolve_shapes(name: str) -> NetSpec:
    """Walk the arch spec and compute every layer's in/out shape (no weights yet)."""
    in_layout, specs = ARCHS[name]
    shape = input_shape(in_layout)
    net = NetSpec(name, in_layout)
    n = len(specs)
    for i, spec in enumerate(specs):
        kind = spec[0]
        c, h, w = shape
        if kind == "conv":
            out_c, (kh, kw) = spec[1], spec[2]
            layer = Layer(L_CONV, True, shape, (out_c, h, w), (kh, kw), (kh // 2, kw // 2))
        elif kind == "dw":
            (kh, kw) = spec[1]
            layer = Layer(L_DWCONV, True, shape, (c, h, w), (kh, kw), (kh // 2, kw // 2))
        elif kind == "pool":
            layer = Layer(L_POOL, False, shape, (c, h // 2, w // 2), (2, 2))
        elif kind == "gap":
            layer = Layer(L_GAP, False, shape, (c, 1, 1))
        elif kind == "dense":
            out = spec[1]
            layer = Layer(L_DENSE, i < n - 1, shape, (out, 1, 1))
        else:
            raise ValueError(kind)
        net.layers.append(layer)
        shape = layer.out_shape
    if shape != (1, 1, 1):
        raise ValueError(f"{name}: network must end in a single logit, ends in {shape}")
    return net


# --- numpy reference (what the C computes) ------------------------------------


def _conv2d(
    x: np.ndarray, w: np.ndarray, b: np.ndarray, pad: tuple[int, int], groups: int
) -> np.ndarray:
    """x (C, H, W), w (O, C/groups, kh, kw) -> (O, H, W), 'same' padding, stride 1."""
    c, h, wd = x.shape
    o, cpg, kh, kw = w.shape
    xp = np.pad(x, ((0, 0), (pad[0], pad[0]), (pad[1], pad[1])))
    out = np.zeros((o, h, wd), dtype=np.float32)
    opg = o // groups
    for g in range(groups):
        xs = xp[g * cpg : (g + 1) * cpg]
        ws = w[g * opg : (g + 1) * opg]
        for ky in range(kh):
            for kx in range(kw):
                patch = xs[:, ky : ky + h, kx : kx + wd]  # (cpg, H, W)
                out[g * opg : (g + 1) * opg] += np.einsum("oc,chw->ohw", ws[:, :, ky, kx], patch)
    return out + b[:, None, None]


def forward_numpy(net: NetSpec, patches: np.ndarray) -> np.ndarray:
    """Logits for (n, 280) patches, in float32 with the C's operation order."""
    xs = patch_to_input(patches, net.input_layout)
    out = np.empty(xs.shape[0], dtype=np.float32)
    for i in range(xs.shape[0]):
        a = xs[i].astype(np.float32)
        for layer in net.layers:
            if layer.kind in (L_CONV, L_DWCONV):
                groups = 1 if layer.kind == L_CONV else a.shape[0]
                a = _conv2d(a, layer.weight, layer.bias, layer.pad, groups)
            elif layer.kind == L_POOL:
                c, h, w = a.shape
                a = (
                    a[:, : (h // 2) * 2, : (w // 2) * 2]
                    .reshape(c, h // 2, 2, w // 2, 2)
                    .max(axis=(2, 4))
                )
            elif layer.kind == L_GAP:
                a = a.mean(axis=(1, 2), dtype=np.float32)[:, None, None]
            elif layer.kind == L_DENSE:
                a = (layer.weight @ a.reshape(-1) + layer.bias)[:, None, None]
            if layer.relu:
                a = np.maximum(a, np.float32(0))
            a = a.astype(np.float32)
        out[i] = a.reshape(-1)[0]
    return out


# --- PyTorch side --------------------------------------------------------------


def build_torch(name: str):
    """nn.Sequential for the arch; layer i of the module list maps to spec layer i."""
    import torch.nn as nn

    net = resolve_shapes(name)
    mods = []
    for layer in net.layers:
        if layer.kind == L_CONV:
            mods.append(
                nn.Conv2d(layer.in_shape[0], layer.out_shape[0], layer.kernel, padding=layer.pad)
            )
        elif layer.kind == L_DWCONV:
            c = layer.in_shape[0]
            mods.append(nn.Conv2d(c, c, layer.kernel, padding=layer.pad, groups=c))
        elif layer.kind == L_POOL:
            mods.append(nn.MaxPool2d(2))
        elif layer.kind == L_GAP:
            mods.append(nn.AdaptiveAvgPool2d(1))
        elif layer.kind == L_DENSE:
            mods.append(nn.Flatten())
            mods.append(nn.Linear(int(np.prod(layer.in_shape)), layer.out_shape[0]))
        if layer.relu:
            mods.append(nn.ReLU())
    return nn.Sequential(*mods), net


def attach_weights(net: NetSpec, module) -> NetSpec:
    """Copy the trained parameters out of the torch module into the spec."""
    import torch.nn as nn

    params = [m for m in module if isinstance(m, nn.Conv2d | nn.Linear)]
    weighted = [layer for layer in net.layers if layer.kind in (L_CONV, L_DWCONV, L_DENSE)]
    if len(params) != len(weighted):
        raise ValueError("module and spec disagree about the number of weighted layers")
    for layer, m in zip(weighted, params, strict=True):
        layer.weight = m.weight.detach().cpu().numpy().astype(np.float32)
        layer.bias = m.bias.detach().cpu().numpy().astype(np.float32)
    return net


@dataclass
class CnnModel:
    """The trained net as the rest of the package sees a model."""

    kind: str  # "cnn"
    name: str
    layout: int  # LAYOUT_LOGMEL
    offset: int  # 0
    net: NetSpec
    meta: dict = field(default_factory=dict)

    @property
    def n_features(self) -> int:
        return N_LOGMEL_PATCH

    def score(self, features: np.ndarray) -> np.ndarray:
        out = forward_numpy(self.net, np.atleast_2d(features))
        return out if np.ndim(features) > 1 else np.float32(out[0])


def train_cnn(
    arch: str,
    x_train: np.ndarray,
    y_train: np.ndarray,
    *,
    epochs: int = 40,
    batch_size: int = 256,
    lr: float = 2e-3,
    weight_decay: float = 1e-4,
    noise_std: float = 0.05,
    holdout: float = 0.1,
    patience: int = 6,
    seed: int = 42,
    log=print,
) -> CnnModel:
    """Adam + BCE-with-logits, class weighted, early stopping on a held-out slice."""
    import torch
    import torch.nn.functional as tf

    torch.manual_seed(seed)
    rng = np.random.default_rng(seed)
    module, net = build_torch(arch)

    n = x_train.shape[0]
    idx = rng.permutation(n)
    n_hold = max(1, int(n * holdout))
    hold, tr = idx[:n_hold], idx[n_hold:]
    xt = torch.from_numpy(patch_to_input(x_train, net.input_layout))
    yt = torch.from_numpy(y_train.astype(np.float32))
    pos = float(yt[tr].sum().item())
    neg = float(tr.size - pos)
    pos_weight = torch.tensor([neg / max(pos, 1.0)], dtype=torch.float32)

    opt = torch.optim.Adam(module.parameters(), lr=lr, weight_decay=weight_decay)
    sched = torch.optim.lr_scheduler.CosineAnnealingLR(opt, T_max=epochs)
    best_loss, best_state, bad = float("inf"), None, 0
    for epoch in range(epochs):
        module.train()
        perm = rng.permutation(tr)
        total = 0.0
        for s in range(0, perm.size, batch_size):
            b = torch.from_numpy(perm[s : s + batch_size])
            xb = xt[b]
            if noise_std > 0:
                xb = xb + torch.randn_like(xb) * noise_std
            logits = module(xb).reshape(-1)
            loss = tf.binary_cross_entropy_with_logits(logits, yt[b], pos_weight=pos_weight)
            opt.zero_grad()
            loss.backward()
            opt.step()
            total += float(loss.item()) * b.numel()
        sched.step()
        module.eval()
        with torch.no_grad():
            hb = torch.from_numpy(hold)
            hl = module(xt[hb]).reshape(-1)
            hloss = float(
                tf.binary_cross_entropy_with_logits(hl, yt[hb], pos_weight=pos_weight).item()
            )
        log(f"    {arch} epoch {epoch + 1:2d} train {total / perm.size:.4f} holdout {hloss:.4f}")
        if hloss < best_loss - 1e-4:
            best_loss, bad = hloss, 0
            best_state = {k: v.detach().clone() for k, v in module.state_dict().items()}
        else:
            bad += 1
            if bad >= patience:
                break
    if best_state is not None:
        module.load_state_dict(best_state)
    module.eval()
    net = attach_weights(net, module)
    meta = {
        "arch": arch,
        "epochs_run": epoch + 1,
        "holdout_loss": best_loss,
        "macs": net.macs,
        "params": net.params,
    }
    return CnnModel("cnn", arch, LAYOUT_LOGMEL, 0, net, meta)


# --- persistence --------------------------------------------------------------


def save_cnn(model: CnnModel, path: Path) -> None:
    """Weights as .npz plus a JSON sidecar; no torch needed to load."""
    arrays = {}
    for i, layer in enumerate(model.net.layers):
        if layer.weight is not None:
            arrays[f"w{i}"] = layer.weight
            arrays[f"b{i}"] = layer.bias
    np.savez(path.with_suffix(".npz"), **arrays)
    side = {
        "kind": model.kind,
        "name": model.name,
        "layout": model.layout,
        "offset": model.offset,
        "input_layout": model.net.input_layout,
        "layers": [
            {k: v for k, v in asdict(layer).items() if k not in ("weight", "bias")}
            for layer in model.net.layers
        ],
        "meta": model.meta,
    }
    path.with_suffix(".json").write_text(json.dumps(side, indent=2, default=list))


def load_cnn(path: Path) -> CnnModel:
    side = json.loads(path.with_suffix(".json").read_text())
    net = resolve_shapes(side["name"])
    with np.load(path.with_suffix(".npz")) as z:
        for i, layer in enumerate(net.layers):
            if f"w{i}" in z:
                layer.weight = z[f"w{i}"]
                layer.bias = z[f"b{i}"]
    return CnnModel(side["kind"], side["name"], side["layout"], side["offset"], net, side["meta"])

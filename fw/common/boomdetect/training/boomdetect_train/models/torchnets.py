"""Networks on the band spectrograms (layouts 1xx/4xx/5xx/6xx), for the 2026-10-07 comparison.

Offline only: plain torch modules with no numpy reference and no C side (models/cnn.py
is the one whose nets reach the board). The architectures follow the TalTech
BEC 2026 paper (Alagiyadura Fernando et al., "Acoustic Drone Detection on
Resource-Constrained Edge Devices") with a single logit instead of their three
classes, plus a mid-sized 2-D CNN of our own:

    cnn_m    Conv2D 16 -> pool -> Conv2D 32 -> pool -> Conv2D 32 -> global average
    crnn1d   their Arch 7: Conv1D 32 -> BN, pool -> Conv1D 16 -> BN, pool -> LSTM 32
    crnn2d   their Arch 8: Conv2D 32 -> BN, pool -> Conv2D 16 -> BN, pool -> LSTM 32
    lstm20   their Arch 14: LSTM 20 over the frames

then Dense (32, 20 for lstm20) + ReLU + dropout and the logit. On a hybrid layout
(5xx/6xx) the 78 numbers of layout 4 (mean-c0 dropped, NaN filled with the
training means, standardised) go through Dense 32 + ReLU and join the network's
own features before the head - "GS added to layout 4".

The patch is mean-removed log power (features.spec_patch); it is divided by one
global standard deviation from the training windows. Every model records its
multiply-accumulates per window, its parameter count and the int8 size of the
weights, for the "would it fit on the node" column.
"""

from __future__ import annotations

import time
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

from boomdetect_train.dsp.spectro import SPEC_N_BANDS
from boomdetect_train.features import HYBRID_AUX_LAYOUT, LAYOUTS, spec_layout

TORCH_ARCHS = ("cnn_m", "crnn1d", "crnn2d", "lstm20")
SCORE_BATCH = 16384


def layout_shape(layout: int) -> tuple[int, int, bool]:
    """(frames, bands, hybrid) of a patch or hybrid layout."""
    spec = spec_layout(layout)
    if spec is None or (spec[0] != "patch" and spec[0] not in HYBRID_AUX_LAYOUT):
        raise ValueError(f"layout {layout} is not a spectrogram patch layout")
    return spec[2], SPEC_N_BANDS, spec[0] in HYBRID_AUX_LAYOUT


def aux_width(layout: int) -> int:
    """Numbers beside the patch that the network reads (the aux layout minus its mean-c0)."""
    t, b, hybrid = layout_shape(layout)
    return LAYOUTS[layout].n_features - t * b - 1 if hybrid else 0


def build_module(arch: str, t: int, b: int, n_aux: int):
    import torch
    import torch.nn as nn

    class Net(nn.Module):
        def __init__(self):
            super().__init__()
            self.arch = arch
            head = 20 if arch == "lstm20" else 32
            if arch == "cnn_m":
                self.body = nn.Sequential(
                    nn.Conv2d(1, 16, 3, padding=1),
                    nn.BatchNorm2d(16),
                    nn.ReLU(),
                    nn.MaxPool2d(2),
                    nn.Conv2d(16, 32, 3, padding=1),
                    nn.BatchNorm2d(32),
                    nn.ReLU(),
                    nn.MaxPool2d(2),
                    nn.Conv2d(32, 32, 3, padding=1),
                    nn.BatchNorm2d(32),
                    nn.ReLU(),
                    nn.AdaptiveAvgPool2d(1),
                )
                emb = 32
            elif arch == "crnn1d":
                self.body = nn.Sequential(
                    nn.Conv1d(b, 32, 3, padding=1),
                    nn.ReLU(),
                    nn.BatchNorm1d(32),
                    nn.MaxPool1d(2),
                    nn.Conv1d(32, 16, 3, padding=1),
                    nn.ReLU(),
                    nn.BatchNorm1d(16),
                    nn.MaxPool1d(2),
                )
                self.rnn = nn.LSTM(16, 32, batch_first=True)
                emb = 32
            elif arch == "crnn2d":
                self.body = nn.Sequential(
                    nn.Conv2d(1, 32, 3, padding=1),
                    nn.ReLU(),
                    nn.BatchNorm2d(32),
                    nn.MaxPool2d(2),
                    nn.Conv2d(32, 16, 3, padding=1),
                    nn.ReLU(),
                    nn.BatchNorm2d(16),
                    nn.MaxPool2d(2),
                )
                self.rnn = nn.LSTM(16 * (b // 4), 32, batch_first=True)
                emb = 32
            elif arch == "lstm20":
                self.rnn = nn.LSTM(b, 20, batch_first=True)
                emb = 20
            else:
                raise ValueError(f"unknown arch {arch!r}: {TORCH_ARCHS}")
            self.aux = nn.Sequential(nn.Linear(n_aux, 32), nn.ReLU()) if n_aux else None
            self.head = nn.Sequential(
                nn.Linear(emb + (32 if n_aux else 0), head),
                nn.ReLU(),
                nn.Dropout(0.3),
                nn.Linear(head, 1),
            )

        def features(self, x):  # x: (N, T, B)
            if self.arch == "cnn_m":
                return self.body(x[:, None]).flatten(1)
            if self.arch == "crnn1d":
                h = self.body(x.transpose(1, 2)).transpose(1, 2)  # (N, T/4, 16)
                return self.rnn(h)[1][0][-1]
            if self.arch == "crnn2d":
                h = self.body(x[:, None])  # (N, 16, T/4, B/4)
                h = h.permute(0, 2, 1, 3).flatten(2)  # (N, T/4, 16 * B/4)
                return self.rnn(h)[1][0][-1]
            return self.rnn(x)[1][0][-1]  # lstm20

        def forward(self, x, aux=None):
            f = self.features(x)
            if self.aux is not None:
                f = torch.cat([f, self.aux(aux)], dim=1)
            return self.head(f).reshape(-1)

    return Net()


def count_cost(module, t: int, b: int, n_aux: int) -> tuple[int, int]:
    """(multiply-accumulates per window, parameters), counted on one dry forward."""
    import torch
    import torch.nn as nn

    macs = 0

    def hook(m, inp, out):
        nonlocal macs
        if isinstance(m, nn.Conv1d | nn.Conv2d):
            k = int(np.prod(m.kernel_size)) * (m.in_channels // m.groups)
            macs += int(out.numel()) * k
        elif isinstance(m, nn.Linear):
            macs += m.in_features * m.out_features
        elif isinstance(m, nn.LSTM):
            steps = int(inp[0].shape[1])
            macs += steps * 4 * (m.input_size * m.hidden_size + m.hidden_size * m.hidden_size)

    hooks = [
        m.register_forward_hook(hook)
        for m in module.modules()
        if isinstance(m, nn.Conv1d | nn.Conv2d | nn.Linear | nn.LSTM)
    ]
    was = module.training
    module.eval()
    with torch.no_grad():
        module(torch.zeros(1, t, b), torch.zeros(1, n_aux) if n_aux else None)
    module.train(was)
    for h in hooks:
        h.remove()
    params = sum(int(p.numel()) for p in module.parameters())
    return macs, params


def _device():
    import torch

    return torch.device("cuda" if torch.cuda.is_available() else "cpu")


@dataclass
class TorchNetModel:
    """A trained network as the rest of the package sees a model (score, layout, offset)."""

    kind: str  # "torch"
    name: str  # the arch
    layout: int
    offset: int  # 0
    state: dict
    norm: dict  # patch_std, aux_mean, aux_std, aux_fill
    meta: dict = field(default_factory=dict)
    _module: object = field(default=None, repr=False)

    @property
    def n_features(self) -> int:
        return LAYOUTS[self.layout].n_features

    def module(self):
        if self._module is None:
            t, b, hybrid = layout_shape(self.layout)
            m = build_module(self.name, t, b, aux_width(self.layout))
            m.load_state_dict(self.state)
            m.eval()
            self._module = m.to(_device())
        return self._module

    def score(self, features: np.ndarray) -> np.ndarray:
        import torch

        x = np.atleast_2d(np.asarray(features, dtype=np.float32))
        m = self.module()
        dev = next(m.parameters()).device
        out = np.empty(x.shape[0], dtype=np.float32)
        with torch.no_grad():
            for s in range(0, x.shape[0], SCORE_BATCH):
                patch, aux = split_inputs(x[s : s + SCORE_BATCH], self.layout, self.norm)
                pt = torch.from_numpy(patch).to(dev)
                at = None if aux is None else torch.from_numpy(aux).to(dev)
                out[s : s + SCORE_BATCH] = m(pt, at).float().cpu().numpy()
        return out if np.ndim(features) > 1 else np.float32(out[0])


def split_inputs(x: np.ndarray, layout: int, norm: dict) -> tuple[np.ndarray, np.ndarray | None]:
    """(normalised patch (N, T, B), normalised layout-4 block (N, 78) or None)."""
    t, b, hybrid = layout_shape(layout)
    patch = (x[:, : t * b] / np.float32(norm["patch_std"])).reshape(-1, t, b).astype(np.float32)
    if not hybrid:
        return patch, None
    aux = x[:, t * b + 1 : t * b + 1 + aux_width(layout)].astype(np.float32)
    fill = np.asarray(norm["aux_fill"], dtype=np.float32)
    aux = np.where(np.isnan(aux), fill, aux)
    aux = (aux - np.asarray(norm["aux_mean"], np.float32)) / np.asarray(norm["aux_std"], np.float32)
    return patch, aux.astype(np.float32)


def fit_norm(x: np.ndarray, layout: int, seed: int) -> dict:
    t, b, hybrid = layout_shape(layout)
    rng = np.random.default_rng(seed)
    pick = rng.choice(x.shape[0], size=min(x.shape[0], 100_000), replace=False)
    norm = {"patch_std": float(np.std(x[pick, : t * b])) or 1.0}
    if hybrid:
        aux = x[:, t * b + 1 : t * b + 1 + aux_width(layout)].astype(np.float64)
        fill = np.nanmean(aux, axis=0)
        fill = np.where(np.isnan(fill), 0.0, fill)
        filled = np.where(np.isnan(aux), fill, aux)
        norm["aux_fill"] = fill.astype(np.float32).tolist()
        norm["aux_mean"] = filled.mean(axis=0).astype(np.float32).tolist()
        norm["aux_std"] = (filled.std(axis=0) + 1e-6).astype(np.float32).tolist()
    return norm


def train_torchnet(
    arch: str,
    layout: int,
    x_train: np.ndarray,
    y_train: np.ndarray,
    *,
    sample_weight: np.ndarray | None = None,
    epochs: int = 20,
    batch_size: int = 2048,
    lr: float = 3e-3,
    weight_decay: float = 1e-4,
    noise_std: float = 0.05,
    holdout: float = 0.1,
    patience: int = 4,
    seed: int = 42,
    log=print,
) -> TorchNetModel:
    """Adam + BCE-with-logits + cosine schedule, early stopping on a held-out slice (as train_cnn).

    The whole training set sits on the device in float16 and batches are indexed
    there - the nets are small, so feeding them is the cost, not computing them.
    """
    import torch
    import torch.nn.functional as tf

    t0 = time.time()
    torch.manual_seed(seed)
    rng = np.random.default_rng(seed)
    dev = _device()
    if dev.type == "cuda":
        torch.cuda.empty_cache()  # whatever the previous fit left in the allocator
    t, b, hybrid = layout_shape(layout)
    n_aux = aux_width(layout)
    module = build_module(arch, t, b, n_aux)
    macs, params = count_cost(module, t, b, n_aux)
    module = module.to(dev)

    norm = fit_norm(x_train, layout, seed)
    n = x_train.shape[0]
    xs_p, xs_a = [], []
    for s in range(0, n, 65536):
        p, a = split_inputs(x_train[s : s + 65536], layout, norm)
        xs_p.append(torch.from_numpy(p).to(dev, torch.float16))
        if a is not None:
            xs_a.append(torch.from_numpy(a).to(dev, torch.float16))
    xp = torch.cat(xs_p)
    xa = torch.cat(xs_a) if xs_a else None
    del xs_p, xs_a
    yt = torch.from_numpy(y_train.astype(np.float32)).to(dev)
    wt = (
        None
        if sample_weight is None
        else torch.from_numpy(np.asarray(sample_weight, np.float32)).to(dev)
    )

    idx = rng.permutation(n)
    n_hold = max(1, int(n * holdout))
    hold = torch.from_numpy(idx[:n_hold]).to(dev)
    tr = idx[n_hold:]
    pos = float(y_train[tr].sum())
    pos_weight = torch.tensor([(tr.size - pos) / max(pos, 1.0)], dtype=torch.float32, device=dev)

    def loss_of(logits, bi):
        if wt is None:
            return tf.binary_cross_entropy_with_logits(logits, yt[bi], pos_weight=pos_weight)
        per = tf.binary_cross_entropy_with_logits(logits, yt[bi], reduction="none")
        return (per * wt[bi]).sum() / wt[bi].sum().clamp_min(1e-12)

    def batch(bi, train: bool):
        xb = xp[bi].float()
        if train and noise_std > 0:
            xb = xb + torch.randn_like(xb) * noise_std
        ab = None if xa is None else xa[bi].float()
        return xb, ab

    opt = torch.optim.Adam(module.parameters(), lr=lr, weight_decay=weight_decay)
    sched = torch.optim.lr_scheduler.CosineAnnealingLR(opt, T_max=epochs)
    best_loss, best_state, bad, epoch = float("inf"), None, 0, 0
    for epoch in range(epochs):
        module.train()
        perm = torch.from_numpy(rng.permutation(tr)).to(dev)
        total = 0.0
        for s in range(0, perm.numel(), batch_size):
            bi = perm[s : s + batch_size]
            if bi.numel() < 2:
                continue
            xb, ab = batch(bi, True)
            loss = loss_of(module(xb, ab), bi)
            opt.zero_grad()
            loss.backward()
            opt.step()
            total += float(loss.detach()) * bi.numel()
        sched.step()
        module.eval()
        with torch.no_grad():
            parts = []
            for s in range(0, hold.numel(), SCORE_BATCH):
                bi = hold[s : s + SCORE_BATCH]
                xb, ab = batch(bi, False)
                parts.append(module(xb, ab))
            hloss = float(loss_of(torch.cat(parts), hold))
        log(
            f"    {arch} epoch {epoch + 1:2d} train {total / max(1, tr.size):.4f}"
            f" holdout {hloss:.4f}"
        )
        if hloss < best_loss - 1e-4:
            best_loss, bad = hloss, 0
            best_state = {k: v.detach().cpu().clone() for k, v in module.state_dict().items()}
        else:
            bad += 1
            if bad >= patience:
                break
    if best_state is None:
        best_state = {k: v.detach().cpu().clone() for k, v in module.state_dict().items()}
    meta = {
        "arch": arch,
        "epochs_run": epoch + 1,
        "holdout_loss": best_loss,
        "macs": macs,
        "params": params,
        "int8_kb": round(params / 1024.0, 1),
        "frames": t,
        "hybrid": hybrid,
        "device": dev.type,
        "seed": seed,
        "train_seconds": round(time.time() - t0, 1),
    }
    return TorchNetModel("torch", arch, layout, 0, best_state, norm, meta)


def save_torchnet(model: TorchNetModel, path: Path) -> None:
    import torch

    torch.save(
        {
            "kind": model.kind,
            "name": model.name,
            "layout": model.layout,
            "offset": model.offset,
            "state": model.state,
            "norm": model.norm,
            "meta": model.meta,
        },
        path.with_suffix(".pt"),
    )


def load_torchnet(path: Path) -> TorchNetModel:
    import torch

    d = torch.load(path.with_suffix(".pt"), map_location="cpu", weights_only=False)
    return TorchNetModel(
        d["kind"], d["name"], d["layout"], d["offset"], d["state"], d["norm"], d["meta"]
    )

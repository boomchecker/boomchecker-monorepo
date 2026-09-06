"""The CNN specs: shapes resolve, torch and the numpy reference agree, budgets hold."""

from __future__ import annotations

import numpy as np
import pytest

from boomdetect_train.models import cnn

torch = pytest.importorskip("torch")

MAC_BUDGET = 1_500_000
PARAM_BUDGET = 64_000  # floats: 256 KB


@pytest.mark.parametrize("arch", sorted(cnn.ARCHS))
def test_shapes_and_budgets(arch):
    net = cnn.resolve_shapes(arch)
    assert net.layers[-1].out_shape == (1, 1, 1)
    assert not net.layers[-1].relu
    assert 0 < net.macs <= MAC_BUDGET, net.macs
    module, _ = cnn.build_torch(arch)
    n_params = sum(p.numel() for p in module.parameters())
    assert n_params <= PARAM_BUDGET, n_params


@pytest.mark.parametrize("arch", sorted(cnn.ARCHS))
def test_torch_and_numpy_reference_agree(arch):
    torch.manual_seed(0)
    module, net = cnn.build_torch(arch)
    net = cnn.attach_weights(net, module)
    rng = np.random.default_rng(0)
    patches = rng.normal(size=(6, 280)).astype(np.float32) * 2.0
    with torch.no_grad():
        want = module(torch.from_numpy(cnn.patch_to_input(patches, net.input_layout))).reshape(-1)
    got = cnn.forward_numpy(net, patches)
    np.testing.assert_allclose(got, want.numpy(), rtol=1e-4, atol=1e-4)
    assert net.params == sum(p.numel() for p in module.parameters())


def test_patch_to_input_layouts():
    p = np.arange(280, dtype=np.float32)[None]
    fm = cnn.patch_to_input(p, cnn.IN_FRAME_MAJOR)
    assert fm.shape == (1, 1, 14, 20)
    assert fm[0, 0, 3, 5] == 3 * 20 + 5
    mc = cnn.patch_to_input(p, cnn.IN_MEL_CHANNELS)
    assert mc.shape == (1, 20, 1, 14)
    assert mc[0, 5, 0, 3] == 3 * 20 + 5


def test_train_saves_and_reloads(tmp_path):
    rng = np.random.default_rng(1)
    n = 400
    y = (rng.random(n) < 0.5).astype(np.int64)
    x = rng.normal(size=(n, 280)).astype(np.float32)
    x[y == 1, :20] += 1.5  # first frame louder in the positives
    model = cnn.train_cnn("cnn_small", x, y, epochs=3, batch_size=64, log=lambda *_: None)
    assert model.layout == 3 and model.offset == 0
    s = model.score(x[:8])
    assert s.shape == (8,) and np.isfinite(s).all()
    cnn.save_cnn(model, tmp_path / "m")
    again = cnn.load_cnn(tmp_path / "m")
    np.testing.assert_array_equal(again.score(x[:8]), s)
    assert again.meta["macs"] == model.net.macs

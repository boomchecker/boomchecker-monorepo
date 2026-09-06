"""Exported headers parse back to the same forward pass the Python computes."""

from __future__ import annotations

import numpy as np
import pytest

from boomdetect_train import export
from boomdetect_train.dsp.tables import parse_arrays, parse_defines
from boomdetect_train.features import LAYOUT_STATS, LAYOUT_STATS_SPECTRAL, LAYOUTS
from boomdetect_train.models.headers import MlpHeader, load_mlp_header, load_svm_header
from boomdetect_train.train import WindowSet, train_gbt, train_mlp, train_svm


def synthetic_window_set(layout: int, n: int = 600, seed: int = 3) -> WindowSet:
    """Two Gaussian blobs in the layout's width, separable but overlapping."""
    rng = np.random.default_rng(seed)
    width = LAYOUTS[layout].n_features
    y = (rng.random(n) < 0.5).astype(np.int64)
    shift = rng.normal(size=width) * 0.8
    x = rng.normal(size=(n, width)) + y[:, None] * shift[None, :]
    x[:, 0] += rng.normal(size=n) * 5.0  # feature 0 carries level; must not matter
    ids = np.asarray([f"clip{i // 10}" for i in range(n)], dtype=str)
    return WindowSet(x.astype(np.float32), y, ids, layout)


@pytest.fixture(scope="module")
def ws():
    return synthetic_window_set(LAYOUT_STATS)


def _tmp_header(tmp_path, text: str, name: str):
    p = tmp_path / name
    p.write_text(text, encoding="utf-8")
    return p


def test_mlp_round_trip(ws, tmp_path):
    model = train_mlp(ws, hidden=(8,))
    text = export.export_mlp(model, "t1", ["test"])
    hdr = load_mlp_header(_tmp_header(tmp_path, text, "mlp.h"), "t1", offset=model.offset)
    assert isinstance(hdr, MlpHeader)
    assert hdr.n_features == 51
    got = hdr.score(ws.x[:50])
    want = model.score(ws.x[:50])
    np.testing.assert_allclose(got, want, rtol=1e-5, atol=1e-5)


def test_linear_round_trip(ws, tmp_path):
    model = train_svm(ws)
    text = export.export_linear(model, "t2", ["test"])
    hdr = load_svm_header(_tmp_header(tmp_path, text, "svm.h"), "t2", offset=model.offset)
    np.testing.assert_allclose(hdr.score(ws.x[:50]), model.score(ws.x[:50]), rtol=1e-5, atol=1e-5)


def test_gbt_flatten_matches_sklearn(ws):
    model = train_gbt(ws, max_iter=30, max_leaf_nodes=7)
    flat = export.flatten_forest(model.model)
    x = ws.x[:64, model.offset :]
    got = export.forest_decision(flat, x)
    want = model.model.decision_function(x.astype(np.float64))
    np.testing.assert_allclose(got, want, rtol=1e-5, atol=1e-5)
    assert flat["roots"].shape[0] == len(model.model._predictors)
    assert (flat["feature"][flat["feature"] != export.GBT_LEAF] < x.shape[1]).all()


def test_gbt_header_parses_and_is_consistent(ws, tmp_path):
    model = train_gbt(ws, max_iter=20, max_leaf_nodes=7)
    text = export.export_gbt(model, "t3", ["test"])
    defines = parse_defines(text)
    arrays = parse_arrays(text, defines)
    assert defines["GBT_NUM_FEATURES"] == 51
    assert defines["GBT_NUM_TREES"] == arrays["gbt_tree_root"].shape[0]
    assert defines["GBT_NUM_NODES"] == arrays["gbt_node_feature"].shape[0]
    # Children point inside the forest and never at a root of a later tree.
    split = arrays["gbt_node_feature"] != export.GBT_LEAF
    assert (arrays["gbt_node_left"][split] < defines["GBT_NUM_NODES"]).all()
    assert (arrays["gbt_node_right"][split] < defines["GBT_NUM_NODES"]).all()
    # Re-evaluate from the parsed arrays, which is what the C does.
    flat = {
        "roots": arrays["gbt_tree_root"],
        "feature": arrays["gbt_node_feature"],
        "threshold": arrays["gbt_node_threshold"],
        "left": arrays["gbt_node_left"],
        "right": arrays["gbt_node_right"],
        "value": arrays["gbt_node_value"],
        "baseline": np.float32(flat_baseline(text)),
    }
    x = ws.x[:40, model.offset :]
    np.testing.assert_allclose(
        export.forest_decision(flat, x), model.score(ws.x[:40]), rtol=1e-4, atol=1e-4
    )


def flat_baseline(text: str) -> float:
    import re

    m = re.search(r"#define GBT_BASELINE ([-+0-9.eE]+)f", text)
    assert m
    return float(m.group(1))


def test_parity_vectors_and_header():
    ws2 = synthetic_window_set(LAYOUT_STATS_SPECTRAL, n=200)
    model = train_svm(ws2)
    vec = export.parity_vectors(model, ws2.x, n=16)
    assert vec.features.shape == (16, 69)
    assert vec.expected.shape == (16,)
    text = export.export_parity_header(
        [("svm_t", LAYOUT_STATS_SPECTRAL, model.offset, vec)], ["test"]
    )
    arrays = parse_arrays(text, parse_defines(text))
    assert arrays["parity_svm_t_features"].shape == (16, 69)
    np.testing.assert_allclose(arrays["parity_svm_t_expected"], vec.expected, rtol=1e-6)
    assert "parity_entries[]" in text
    assert '"svm_t", 2u, 69u, 16u' in text


def test_c_float_round_trips_float32():
    for v in (0.0, 1.0, -3.70808077, 1e-7, 123456.789, np.float32(0.1)):
        s = export._c_float(v)
        assert s.endswith("f")
        assert np.float32(float(s[:-1])) == np.float32(v)

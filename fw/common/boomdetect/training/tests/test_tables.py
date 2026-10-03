"""The parsed tables are the firmware's, and they have the shape they claim."""

from __future__ import annotations

import numpy as np

from boomdetect_train.dsp.tables import parse_arrays, parse_defines


def test_dimensions(tables):
    assert tables.n_fft == 1024
    assert tables.n_mels == 20
    assert tables.n_mfcc == 13
    assert tables.n_bins == 513
    assert tables.filter_coefs.shape[0] == int(tables.filter_len.sum()) == 943


def test_window_is_symmetric_hamming(tables):
    w = tables.window.astype(np.float64)
    n = np.arange(1024)
    ref = 0.54 - 0.46 * np.cos(2.0 * np.pi * n / 1023)
    assert abs(w[0] - 0.08) < 1e-6
    assert np.allclose(w, ref, atol=2e-7)
    assert np.allclose(w, w[::-1], atol=1e-7)


def test_dct_is_orthonormal_type_ii(tables):
    d = tables.dct.astype(np.float64)
    # Row 0 of an orthonormal DCT-II is 1/sqrt(N) everywhere.
    assert np.allclose(d[0], 1.0 / np.sqrt(20), atol=1e-6)
    # Rows are orthonormal.
    assert np.allclose(d @ d.T, np.eye(13), atol=1e-5)


def test_mel_filters_cover_the_band_without_reaching_dc(tables):
    m = tables.mel_matrix()
    assert m.shape == (20, 513)
    assert (m[:, 0] == 0).all(), "a filter reaches the DC bin"
    last = int(tables.filter_pos[-1] + tables.filter_len[-1])
    assert last <= 513
    # Slaney-normalised triangles: every filter has positive area.
    assert (m.sum(axis=1) > 0).all()


def test_parser_handles_nested_and_macro_dimensions():
    text = """
#define ROWS 2
#define COLS 3
static const float grid[ROWS][COLS] = {
    {1.0f, 2.5e-1f, -3.0e+00f},
    {4.0f, 5.0f, 6.0f}
};
static const uint32_t pos[3] = { 1, 10, 100 };
"""
    d = parse_defines(text)
    a = parse_arrays(text, d)
    assert a["grid"].shape == (2, 3)
    assert a["grid"].dtype == np.float32
    assert float(a["grid"][0, 2]) == -3.0
    assert a["pos"].tolist() == [1, 10, 100]

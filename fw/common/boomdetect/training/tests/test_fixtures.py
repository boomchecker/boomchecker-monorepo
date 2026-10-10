"""The C headers the Python specification generates are the ones checked in."""

from __future__ import annotations

import numpy as np

from boomdetect_train import fixtures as fx
from boomdetect_train.dsp.selftest import AM_PERIOD, am_lcg_signal, am_table, lcg_signal
from boomdetect_train.paths import VECTORS_DIR


def _without_stamp(text: str) -> str:
    return "\n".join(line for line in text.splitlines() if not line.startswith(" * generated:"))


def test_envelope_coefs_header_is_current():
    want = fx.ENV_COEFS_H.read_text(encoding="utf-8")
    assert _without_stamp(want) == _without_stamp(fx.envelope_coefs_text()), (
        "src/envelope_coefs.h is stale: run `bdtrain fixtures`"
    )


def test_modulation_fixture_is_current():
    want = (VECTORS_DIR / "extractor_mod_expected.h").read_text(encoding="utf-8")
    assert _without_stamp(want) == _without_stamp(fx.mod_fixture_text()), (
        "tests/vectors/extractor_mod_expected.h is stale: run `bdtrain fixtures`"
    )


def test_am_signal_truncates_like_c():
    n = 3000
    base = lcg_signal(n).astype(np.int64)
    table = am_table()
    assert table.shape == (AM_PERIOD,) and table.min() == -512 and table.max() == 512
    ref = np.array(
        [int(int(v) * (1024 + int(table[i % AM_PERIOD])) / 1024) for i, v in enumerate(base)],
        dtype=np.int16,
    )
    np.testing.assert_array_equal(am_lcg_signal(n), ref)

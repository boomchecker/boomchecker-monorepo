"""The partition: its three fractions, and that it is decided by the group.

A split is only worth reporting if a recording cannot appear on both sides of
it, so what is tested here is the group key as much as the fractions.
"""

from __future__ import annotations

import pandas as pd
import pytest

from boomdetect_train.datasets.manifest import (
    COLUMNS,
    TEST_FRACTION,
    VAL_FRACTION,
    save_manifest,
    split_for_group,
    summarize,
)
from boomdetect_train.datasets.sources import HF_GROUP_CLIPS, hf_group


def test_split_fractions_are_what_they_claim():
    n = 20000
    got = pd.Series([split_for_group(f"g/{i}") for i in range(n)]).value_counts(normalize=True)
    assert got["test"] == pytest.approx(TEST_FRACTION, abs=0.02)
    assert got["val"] == pytest.approx(VAL_FRACTION, abs=0.02)
    assert got["train"] == pytest.approx(1.0 - TEST_FRACTION - VAL_FRACTION, abs=0.02)


def test_hf_group_holds_a_block_of_consecutive_clips_together():
    block = HF_GROUP_CLIPS["drone"]
    base = 5 * block
    inside = {hf_group(f"drone-{base + k}", "fallback") for k in range(block)}
    assert len(inside) == 1, "one block must be one group"
    assert hf_group(f"drone-{base + block}", "fallback") not in inside
    assert hf_group("no-drone-7427", "fb") == hf_group("no-drone-7440", "fb")


def test_hf_group_falls_back_on_an_unexpected_name():
    assert hf_group("row1234", "hf_drone_audio/shard/row1234") == "hf_drone_audio/shard/row1234"


def test_summarize_counts_the_new_split(tmp_path):
    df = pd.DataFrame(
        [
            {c: v for c, v in zip(COLUMNS, row, strict=True)}
            for row in (
                ("a", "s", "p", 1, "drone", "g1", "train", "train", 16000, 1.0),
                ("b", "s", "p", 0, "noise", "g2", "test", "train", 16000, 3.0),
            )
        ]
    )
    save_manifest(df, tmp_path / "m.parquet")
    out = summarize(df)
    assert set(out["split"]) == {"train", "test"}
    assert out.loc[out["split"] == "test", "hours"].iloc[0] == pytest.approx(3.0 / 3600)

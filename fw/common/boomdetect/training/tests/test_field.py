"""The field source: what a session directory becomes in the manifest, and the folds.

A field folder is one recording, so everything here is about keeping it one
thing - its runs share a group, its chunks come back in order - while the
start transient and the hand-deleted chunks are handled the way the board's
streams need.
"""

from __future__ import annotations

import csv
from pathlib import Path

import numpy as np
import pytest
import soundfile as sf

from boomdetect_train.datasets.field import (
    START_SKIP_S,
    assign_folds,
    chunk_runs,
    drone_of,
    field_rows,
    is_field_path,
    load_field_audio,
)
from boomdetect_train.datasets.manifest import ROLE_FIELD
from boomdetect_train.train import WindowSet, share_weights

SR = 48000


def _chunk(folder: Path, n: int, seconds: float = 1.0) -> None:
    """chunk-NNN.wav whose samples all equal 100 * n, so order is visible."""
    folder.mkdir(parents=True, exist_ok=True)
    x = np.full(int(seconds * SR), 100 * n, dtype=np.int16)
    sf.write(str(folder / f"chunk-{n:03d}.wav"), x, SR, subtype="PCM_16")


def _index(folder: Path, streams: dict[int, int]) -> None:
    with open(folder / "index.csv", "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["chunk", "file", "stream", "seconds", "samples", "overrun", "err"])
        for n, s in sorted(streams.items()):
            w.writerow([n, f"chunk-{n:03d}.wav", s, "1.000", SR, 0, 0])


@pytest.fixture
def session(tmp_path: Path) -> Path:
    root = tmp_path / "field"
    s = root / "2026-01-01"
    dji = s / "Positive" / "dji_blizko"
    for n in (2, 3, 4):  # chunk 1 (transient) and 5 deleted by hand
        _chunk(dji, n)
    _index(dji, {n: 1 for n in range(1, 6)})
    sf.write(str(s / "Positive" / "runner_zboku.wav"), np.zeros(int(1.5 * SR), np.int16), SR)
    ticho = s / "Negative" / "ticho"
    for n in (1, 2, 3):  # starts at the stream's first sample, no index.csv
        _chunk(ticho, n)
    gap = s / "Negative" / "tleskani"
    for n in (2, 3, 5, 6):
        _chunk(gap, n)
    (s / "notes.txt").write_text("not audio")
    _chunk(s / "Unsorted" / "x", 1)  # neither Positive nor Negative: ignored
    return root


def test_drone_is_read_from_the_name_prefix():
    assert drone_of("dji_nahoruadolu") == "dji_phantom4"
    assert drone_of("runner_priblizeninebonevim2") == "runner250"
    assert drone_of("Runner-far") == "runner250"
    assert drone_of("bebop_hover") == "drone"


def test_runs_end_at_a_missing_chunk_and_at_a_new_stream(tmp_path: Path):
    d = tmp_path / "rec"
    for n in range(1, 7):
        _chunk(d, n)
    _index(d, {1: 1, 2: 1, 3: 1, 4: 2, 5: 2, 6: 2})
    assert chunk_runs(d) == [(1, 3), (4, 6)]
    (d / "chunk-005.wav").unlink()
    assert chunk_runs(d) == [(1, 3), (4, 4), (6, 6)]


def test_rows_label_group_and_categorise_every_recording(session: Path):
    rows = {r["id"]: r for r in field_rows(session, ROLE_FIELD)}
    assert set(rows) == {
        "field/2026-01-01/dji_blizko",
        "field/2026-01-01/runner_zboku",
        "field/2026-01-01/ticho",
        "field/2026-01-01/tleskani/run1",
        "field/2026-01-01/tleskani/run2",
    }
    dji = rows["field/2026-01-01/dji_blizko"]
    assert (dji["label"], dji["category"], dji["role"]) == (1, "dji_phantom4", ROLE_FIELD)
    assert dji["duration"] == pytest.approx(3.0)
    assert rows["field/2026-01-01/runner_zboku"]["category"] == "runner250"
    assert rows["field/2026-01-01/runner_zboku"]["duration"] == pytest.approx(1.5 - START_SKIP_S)
    ticho = rows["field/2026-01-01/ticho"]
    assert (ticho["label"], ticho["category"]) == (0, "ticho")
    assert ticho["duration"] == pytest.approx(3.0 - START_SKIP_S)
    runs = [rows[f"field/2026-01-01/tleskani/run{k}"] for k in (1, 2)]
    assert {r["group"] for r in runs} == {"field/2026-01-01/tleskani"}, "one recording, one group"


def test_a_run_comes_back_in_chunk_order_and_loses_only_the_stream_start(session: Path):
    rows = {r["id"]: r for r in field_rows(session, ROLE_FIELD)}
    x, sr = load_field_audio(rows["field/2026-01-01/dji_blizko"]["path"])
    assert sr == SR and x.shape[0] == 3 * SR, "chunk 2 is not a stream start: nothing cut"
    assert [round(float(x[k * SR]) * 32768) for k in range(3)] == [200, 300, 400]
    y, _ = load_field_audio(rows["field/2026-01-01/ticho"]["path"])
    assert y.shape[0] == 3 * SR - int(START_SKIP_S * SR)
    assert round(float(y[0]) * 32768) == 100


def test_field_paths_are_told_apart_from_parquet_rows():
    assert is_field_path("C:/x/rec#chunks=2-9")
    assert is_field_path("/x/rec.wav#skip=0.2")
    assert not is_field_path("/x/train-00003-of-00039.parquet#12")
    assert not is_field_path("/x/plain.wav")


def test_folds_hold_every_stratum_and_do_not_move():
    strata = {f"dji/{i}": "dji" for i in range(6)}
    strata |= {f"runner/{i}": "runner" for i in range(11)}
    strata |= {f"neg/{i}": "negative" for i in range(11)}
    folds = assign_folds(strata, 4)
    assert folds == assign_folds(dict(reversed(list(strata.items()))), 4)
    sizes = np.bincount(list(folds.values()), minlength=4)
    assert sizes.max() - sizes.min() <= 1
    for k in range(4):
        members = {strata[g] for g, f in folds.items() if f == k}
        assert members == {"dji", "runner", "negative"}, f"fold {k} lacks a stratum"


def _ws(sources: list[tuple[str, int, int]]) -> WindowSet:
    src, y = [], []
    for s, label, n in sources:
        src += [s] * n
        y += [label] * n
    n = len(y)
    return WindowSet(
        np.zeros((n, 52), np.float32), np.asarray(y), np.asarray(src), 1, np.asarray(src)
    )


def test_a_share_is_the_fraction_of_its_class_a_source_carries():
    ws = _ws([("hf", 1, 100), ("dad", 1, 10), ("field", 1, 5), ("hf", 0, 200), ("field", 0, 5)])
    w = share_weights(ws, {"field": 0.25, "dad": 0.15})
    assert w.mean() == pytest.approx(1.0)
    pos, neg = ws.y == 1, ws.y == 0
    assert w[pos].sum() == pytest.approx(w[neg].sum()), "classes balanced by weight"
    frac = {s: w[pos & (ws.source == s)].sum() / w[pos].sum() for s in ("hf", "dad", "field")}
    assert frac == pytest.approx({"hf": 0.60, "dad": 0.15, "field": 0.25})
    assert w[neg & (ws.source == "field")].sum() / w[neg].sum() == pytest.approx(0.25)


def test_shares_that_leave_nothing_or_are_not_fractions_are_refused():
    ws = _ws([("hf", 1, 10), ("field", 1, 5), ("hf", 0, 10)])
    with pytest.raises(ValueError):
        share_weights(ws, {"field": 1.0})
    with pytest.raises(ValueError):
        share_weights(ws, {"field": -0.1})

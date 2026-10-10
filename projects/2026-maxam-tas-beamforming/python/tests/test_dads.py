import io
import json
from pathlib import Path

import huggingface_hub
import numpy as np
import pyarrow as pa
import pyarrow.parquet as pq
import pytest
import soundfile as sf

from beamforming import dads
from beamforming import signals as sg


def test_band_fraction():
    t = np.arange(sg.FS) / sg.FS
    assert dads.band_fraction(np.sin(2 * np.pi * 1000 * t)) > 0.99
    assert dads.band_fraction(np.sin(2 * np.pi * 5000 * t)) < 0.01
    assert dads.band_fraction(np.zeros(100)) == 0.0


def test_random_offset_keeps_margin(rng):
    for n in (dads.MIN_SAMPLES, 8000, 16_384):
        offsets = [dads.random_offset(n, rng) for _ in range(200)]
        assert min(offsets) >= sg.MARGIN
        assert max(offsets) + sg.SEGMENT + sg.MARGIN <= n
    with pytest.raises(ValueError):
        dads.random_offset(dads.MIN_SAMPLES - 1, rng)


def test_list_clips_without_data_explains_fix(tmp_path):
    with pytest.raises(FileNotFoundError, match="task sim:fetch"):
        dads.list_clips(tmp_path)


def test_cached_clips_are_usable(drone_clips):
    manifest = json.loads(
        (Path(__file__).resolve().parents[2] / "data/dads/manifest.json").read_text()
    )
    assert len(drone_clips) == manifest["n"] >= 1
    for x in drone_clips:
        assert len(x) >= dads.MIN_SAMPLES
        assert np.abs(x).max() <= 1.0
        assert dads.band_fraction(x) >= dads.MIN_BAND_FRACTION


def wav_bytes(freq: float) -> bytes:
    t = np.arange(8000) / sg.FS
    x = 0.3 * np.sin(2 * np.pi * freq * t) + 0.2 * np.sin(2 * np.pi * 1.7 * freq * t)
    buf = io.BytesIO()
    sf.write(buf, x, sg.FS, format="WAV", subtype="PCM_16")
    return buf.getvalue()


@pytest.fixture
def fake_hub(tmp_path, monkeypatch):
    """Local parquet (20 clips, 4 row groups) behind a fake ``HfFileSystem``; records tokens."""
    rows = [{"bytes": wav_bytes(500 + 20 * i), "path": f"drone-{i}.wav"} for i in range(20)]
    table = pa.table({"audio": rows, "label": [1] * 20})
    parquet = tmp_path / "shard.parquet"
    pq.write_table(table, parquet, row_group_size=5)
    tokens: list[str | None] = []

    class FakeFs:
        def __init__(self, token=None):
            tokens.append(token)

        def open(self, path, block_size=None):
            return parquet.open("rb")

    monkeypatch.setattr(huggingface_hub, "HfFileSystem", FakeFs)
    return tokens


def test_fetch_writes_complete_cache(tmp_path, fake_hub):
    out = tmp_path / "cache" / "dads"
    manifest = dads.fetch(4, out, row_groups=4)
    assert manifest["n"] == 4
    assert sorted(p.name for p in out.iterdir()) == [
        "000.wav",
        "001.wav",
        "002.wav",
        "003.wav",
        "manifest.json",
    ]
    clips = dads.list_clips(out)
    assert len(clips) == 4
    assert all(len(dads.load_clip(c)) == 8000 for c in clips)
    assert not (out.parent / ".dads.tmp").exists()


def test_failed_fetch_keeps_previous_cache(tmp_path, fake_hub):
    out = tmp_path / "dads"
    dads.fetch(4, out, row_groups=4)
    before = {p.name: p.read_bytes() for p in out.iterdir()}
    with pytest.raises(RuntimeError, match="usable clips"):
        dads.fetch(25, out, row_groups=4)  # the shard has only 20 clips
    assert {p.name: p.read_bytes() for p in out.iterdir()} == before
    assert not (tmp_path / ".dads.tmp").exists()


def test_smaller_refetch_leaves_no_orphans(tmp_path, fake_hub):
    out = tmp_path / "dads"
    dads.fetch(4, out, row_groups=4)
    dads.fetch(2, out, row_groups=4)
    assert sorted(p.name for p in out.iterdir()) == ["000.wav", "001.wav", "manifest.json"]
    assert json.loads((out / "manifest.json").read_text())["n"] == 2


@pytest.mark.parametrize(
    ("env", "expected"),
    [("", None), ("hf_your_token_here", None), ("hf_realtoken123", "hf_realtoken123")],
)
def test_placeholder_token_is_ignored(tmp_path, fake_hub, monkeypatch, env, expected):
    monkeypatch.setenv("HF_TOKEN", env)
    dads.fetch(2, tmp_path / "dads", row_groups=4)
    assert fake_hub == [expected]


def test_cache_ok_detects_missing_and_wrong_n(tmp_path, fake_hub):
    out = tmp_path / "dads"
    assert not dads.cache_ok(out, 4)
    dads.fetch(4, out, row_groups=4)
    assert dads.cache_ok(out, 4) and dads.cache_ok(out)
    assert not dads.cache_ok(out, 5)
    (out / "002.wav").unlink()
    assert not dads.cache_ok(out, 4)

"""WAV writer produces valid, correctly-formatted files."""

from __future__ import annotations

import wave

from stm32node_cli.audio.wav import timestamped_path, write_wav


def test_write_wav_roundtrip(tmp_path):
    pcm = bytes(range(256)) * 8  # 2048 bytes = 1024 samples
    path = write_wav(tmp_path / "out.wav", pcm, sample_rate=48000, channels=1)
    assert path.exists()

    with wave.open(str(path), "rb") as wf:
        assert wf.getnchannels() == 1
        assert wf.getsampwidth() == 2
        assert wf.getframerate() == 48000
        assert wf.getnframes() == 1024
        assert wf.readframes(1024) == pcm


def test_write_wav_creates_parent_dir(tmp_path):
    target = tmp_path / "nested" / "deep" / "out.wav"
    write_wav(target, b"\x00\x00" * 10)
    assert target.exists()


def test_timestamped_path_shape():
    p = timestamped_path("/tmp/recordings", prefix="rec")
    assert p.name.startswith("rec-")
    assert p.suffix == ".wav"
    assert str(p.parent) == "/tmp/recordings"

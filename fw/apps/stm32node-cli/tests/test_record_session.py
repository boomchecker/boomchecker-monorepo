"""RecordSession streams PCM and writes a WAV end to end (fake transport)."""

from __future__ import annotations

import wave

from conftest import FakeTransport
from stm32node_cli.protocol.client import DeviceClient
from stm32node_cli.protocol.codec import pack_header
from stm32node_cli.sessions.record import RecordSession


def test_record_writes_wav(tmp_path):
    payload = bytes(range(256)) * 16  # 4096 bytes
    t = FakeTransport(to_read=pack_header(len(payload)) + payload)
    client = DeviceClient(t)
    session = RecordSession(client, tmp_path)

    result = session.record(1)

    assert result.path.exists()
    assert result.byte_length == len(payload)
    assert result.sample_count == len(payload) // 2

    with wave.open(str(result.path), "rb") as wf:
        assert wf.getframerate() == 48000
        assert wf.readframes(wf.getnframes()) == payload


def test_record_reports_progress(tmp_path):
    payload = b"\x01\x02" * 2048  # 4096 bytes
    t = FakeTransport(to_read=pack_header(len(payload)) + payload)
    session = RecordSession(DeviceClient(t), tmp_path)

    seen: list[tuple[int, int]] = []
    session.record(1, on_progress=lambda done, total: seen.append((done, total)))

    assert seen, "expected at least one progress callback"
    assert seen[-1] == (len(payload), len(payload))

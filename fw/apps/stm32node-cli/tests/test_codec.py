"""Codec round-trips and validation."""

from __future__ import annotations

import pytest

from stm32node_cli.protocol import codec
from stm32node_cli.protocol.spec import HEADER_SIZE, MAGIC, SAMPLE_RATE_HZ


def test_encode_command_formats_line():
    assert codec.encode_command("version") == b"version\n"
    assert codec.encode_command("stream", 5) == b"stream 5\n"


def test_header_round_trip():
    raw = codec.pack_header(4096, channels=1, sample_rate=SAMPLE_RATE_HZ)
    assert len(raw) == HEADER_SIZE
    header = codec.parse_header(raw)
    assert header.channels == 1
    assert header.sample_rate == SAMPLE_RATE_HZ
    assert header.byte_length == 4096
    assert header.sample_count == 2048  # 4096 bytes / 2 bytes-per-sample


def test_parse_header_rejects_bad_magic():
    raw = bytearray(codec.pack_header(10))
    raw[0:4] = b"XXXX"
    with pytest.raises(codec.ProtocolError):
        codec.parse_header(bytes(raw))


def test_parse_header_rejects_wrong_length():
    with pytest.raises(codec.ProtocolError):
        codec.parse_header(MAGIC)  # too short


def test_header_duration():
    # 1 second mono @ 48 kHz, 16-bit = 96000 bytes
    header = codec.parse_header(codec.pack_header(SAMPLE_RATE_HZ * 2))
    assert header.duration_s == pytest.approx(1.0)

"""Parsing tests for boom_test.protocol - no hardware needed."""

from __future__ import annotations

import pytest

from boom_test.protocol import (
    RadioPingError,
    RxFrame,
    clean_line,
    is_radio_ready,
    parse_ping_sent,
    parse_rx_line,
)


def test_clean_line_strips_ansi_and_prompt() -> None:
    assert clean_line('\x1b[s\x1b[u> radio rx: "hi"') == 'radio rx: "hi"'


@pytest.mark.parametrize(
    ("line", "expected"),
    [
        (
            'radio rx: "PING" (4 bytes, RSSI -42.0 dBm, SNR 7.5 dB)',
            RxFrame(text="PING", truncated=False, byte_count=4, rssi_dbm=-42.0, snr_db=7.5),
        ),
        (
            'radio rx: "abc..."... (67 bytes, RSSI -110.5 dBm, SNR -3.0 dB)',
            RxFrame(
                text="abc...", truncated=True, byte_count=67, rssi_dbm=-110.5, snr_db=-3.0
            ),
        ),
    ],
)
def test_parse_rx_line_matches(line: str, expected: RxFrame) -> None:
    assert parse_rx_line(line) == expected


def test_parse_rx_line_ignores_unrelated_text() -> None:
    assert parse_rx_line("radio: ready  869.525 MHz  BW 125.0 kHz  SF7 CR4/5  14 dBm") is None


def test_parse_ping_sent_ok() -> None:
    assert parse_ping_sent('radio ping: sent "PING" (4 bytes)') == "PING"


def test_parse_ping_sent_failure_raises() -> None:
    with pytest.raises(RadioPingError):
        parse_ping_sent("radio ping: failed (error -2)")


def test_parse_ping_sent_ignores_unrelated_text() -> None:
    assert parse_ping_sent("radio: stats reset") is None


def test_is_radio_ready() -> None:
    assert is_radio_ready(["radio: ready  869.525 MHz  BW 125.0 kHz  SF7 CR4/5  14 dBm"])
    assert not is_radio_ready(["radio: not ready (error -1)"])
    assert not is_radio_ready([])

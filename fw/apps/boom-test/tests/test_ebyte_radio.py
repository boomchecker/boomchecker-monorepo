"""boomlink.md section 15.3 scenario 1: raw RadioLib/EBYTE ping/pong.

Send `radio ping` from one board and watch the other's console for the
`radio rx: ...` line cli.c prints for every received packet. Run in both
directions, since a one-way pass can hide a TX or RX fault on either side.
"""

from __future__ import annotations

import uuid

import pytest

from boom_test.board import Board

pytestmark = pytest.mark.hil

# A unique token per test run/direction so a stale/duplicate packet from an
# earlier test can't be mistaken for this one's reply.
_TOKEN_PREFIX = "HIL"


def _unique_payload() -> str:
    return f"{_TOKEN_PREFIX}{uuid.uuid4().hex[:8]}"


def test_master_radio_ready(master: Board) -> None:
    assert master.is_radio_ready(), f"master radio not ready: {master.radio_status()}"


def test_slave_radio_ready(slave: Board) -> None:
    assert slave.is_radio_ready(), f"slave radio not ready: {slave.radio_status()}"


def test_ping_pong_master_to_slave(master: Board, slave: Board) -> None:
    payload = _unique_payload()
    sent = master.radio_ping(payload)
    assert sent == payload

    frame = slave.wait_for_rx(timeout=5.0)
    assert frame is not None, "slave did not report a received packet in time"
    assert frame.text == payload


def test_ping_pong_slave_to_master(master: Board, slave: Board) -> None:
    payload = _unique_payload()
    sent = slave.radio_ping(payload)
    assert sent == payload

    frame = master.wait_for_rx(timeout=5.0)
    assert frame is not None, "master did not report a received packet in time"
    assert frame.text == payload

"""Pairing: the two boards resolve to distinct, live master/slave roles."""

from __future__ import annotations

import pytest

from boom_test.board import Board

pytestmark = pytest.mark.hil


def test_master_and_slave_are_distinct_ports(master_port: str, slave_port: str) -> None:
    assert master_port != slave_port


def test_master_and_slave_respond_to_version(master: Board, slave: Board) -> None:
    assert master.version()
    assert slave.version()

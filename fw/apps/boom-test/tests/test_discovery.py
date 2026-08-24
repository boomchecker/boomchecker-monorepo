"""Device discovery: the HIL rig can find the two connected boards."""

from __future__ import annotations

import pytest

from boom_test.discovery import discover_boards

pytestmark = pytest.mark.hil


def test_discovers_two_boomchecker_boards(request: pytest.FixtureRequest) -> None:
    if request.config.getoption("--master-port") or request.config.getoption("--slave-port"):
        pytest.skip("explicit --master-port/--slave-port given; auto-discovery not in use")

    boards = discover_boards()
    if not boards:
        pytest.skip(
            "no boomchecker-node boards found (USB VID:PID 0483:5710); connect master + slave"
        )
    assert len(boards) == 2, (
        f"expected exactly 2 boomchecker-node boards, found {len(boards)}: "
        f"{[b.device for b in boards]}"
    )

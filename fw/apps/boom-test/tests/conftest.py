"""Fixtures: discover/pair two boards and open a master + slave connection.

`--master-port`/`--slave-port` override auto-discovery for a rig where two
boards are always wired to fixed ports (or where more than two are attached
and discovery alone can't disambiguate). Give both or neither.
"""

from __future__ import annotations

from collections.abc import Iterator

import pytest

from boom_test.board import Board
from boom_test.discovery import BoardPort, assign_roles, discover_boards


def pytest_addoption(parser: pytest.Parser) -> None:
    group = parser.getgroup("boom-test")
    group.addoption(
        "--master-port",
        action="store",
        default=None,
        help="Serial device for the master board, e.g. /dev/ttyACM0. Skips auto-discovery.",
    )
    group.addoption(
        "--slave-port",
        action="store",
        default=None,
        help="Serial device for the slave board, e.g. /dev/ttyACM1. Skips auto-discovery.",
    )


@pytest.fixture(scope="session")
def explicit_ports(request: pytest.FixtureRequest) -> tuple[str, str] | None:
    master = request.config.getoption("--master-port")
    slave = request.config.getoption("--slave-port")
    if master and slave:
        return master, slave
    if master or slave:
        pytest.fail("pass both --master-port and --slave-port, or neither (for auto-discovery)")
    return None


@pytest.fixture(scope="session")
def discovered_boards() -> list[BoardPort]:
    return discover_boards()


@pytest.fixture(scope="session")
def board_ports(
    explicit_ports: tuple[str, str] | None, discovered_boards: list[BoardPort]
) -> tuple[str, str]:
    if explicit_ports is not None:
        return explicit_ports
    if len(discovered_boards) < 2:
        pytest.skip(
            "need 2 boomchecker-node boards (USB VID:PID 0483:5710) connected for HIL tests; "
            f"found {len(discovered_boards)}. Connect master + slave, or pass "
            "--master-port/--slave-port explicitly."
        )
    if len(discovered_boards) > 2:
        pytest.skip(
            f"found {len(discovered_boards)} boomchecker-node boards, expected exactly 2 "
            "for this HIL run; disambiguate with --master-port/--slave-port."
        )
    master, slave = assign_roles(discovered_boards)
    return master.device, slave.device


@pytest.fixture(scope="session")
def master_port(board_ports: tuple[str, str]) -> str:
    return board_ports[0]


@pytest.fixture(scope="session")
def slave_port(board_ports: tuple[str, str]) -> str:
    return board_ports[1]


@pytest.fixture(scope="session")
def master(master_port: str) -> Iterator[Board]:
    with Board(master_port) as board:
        yield board


@pytest.fixture(scope="session")
def slave(slave_port: str) -> Iterator[Board]:
    with Board(slave_port) as board:
        yield board

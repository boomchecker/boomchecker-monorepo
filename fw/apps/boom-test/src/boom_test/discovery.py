"""Find connected boomchecker-node boards and pair two of them for a HIL run.

"Pairing" here is a test-harness concept only: which physical board acts as
`master` versus `slave` for one test run, so scenarios can be written without
an operator labelling boards by hand. BoomLink itself has no master address
(docs/firmware/bom-stm32node/boomlink.md section 7.2) - the roles exist so
scenarios can say "master" and "slave" instead of juggling two raw ports.
"""

from __future__ import annotations

import sys
from dataclasses import dataclass

from serial.tools import list_ports

# ST's sample CDC ACM identity (see fw/bom-stm32node/host/boomchecker-node.inf) -
# every boomchecker-node board enumerates with this VID:PID today.
BOOMCHECKER_VID = 0x0483
BOOMCHECKER_PID = 0x5710


@dataclass(frozen=True)
class BoardPort:
    """One connected boomchecker-node board, before any role is assigned."""

    device: str
    serial_number: str | None
    description: str


def discover_boards() -> list[BoardPort]:
    """Enumerate serial ports matching the boomchecker-node USB identity."""
    boards = [
        BoardPort(p.device, p.serial_number, p.description or "")
        for p in list_ports.comports()
        if p.vid == BOOMCHECKER_VID and p.pid == BOOMCHECKER_PID
    ]
    return sorted(boards, key=lambda b: (b.serial_number or "", b.device))


def assign_roles(boards: list[BoardPort]) -> tuple[BoardPort, BoardPort]:
    """Pick (master, slave) out of exactly two discovered boards.

    Which physical board ends up master is arbitrary but stable across runs -
    sorted by serial number (falling back to device path so two boards
    without a programmed serial number still get a deterministic order).
    """
    if len(boards) != 2:
        raise ValueError(f"assign_roles() needs exactly 2 boards, got {len(boards)}")
    master, slave = sorted(boards, key=lambda b: (b.serial_number or "", b.device))
    return master, slave


def main() -> None:
    """`boom-test-ports` / `python -m boom_test.discovery`: list connected boards."""
    boards = discover_boards()
    if not boards:
        vid_pid = f"{BOOMCHECKER_VID:04x}:{BOOMCHECKER_PID:04x}"
        print(f"no boomchecker-node boards found (USB VID:PID {vid_pid})")
        sys.exit(1)
    for board in boards:
        print(f"{board.device}  serial={board.serial_number or '?'}  {board.description}")


if __name__ == "__main__":
    main()

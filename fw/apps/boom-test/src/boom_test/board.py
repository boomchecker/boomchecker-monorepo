"""One connected board's text CLI (cli.c), over its USB CDC ACM serial port."""

from __future__ import annotations

import time
from types import TracebackType

import serial

from .protocol import RxFrame, clean_line, is_echo, is_radio_ready, parse_ping_sent, parse_rx_line


class BoardError(RuntimeError):
    """Raised when a board does not answer a command at all."""


class Board:
    """Talks to one board's CLI. Baud rate is meaningless for CDC ACM but
    pyserial requires a value.

    Reading a "line" waits up to the port's timeout for the terminator; a
    line split across two reads by an unlucky timeout is not reassembled -
    acceptable for this bring-up harness since cli.c prints each line in one
    burst, but worth knowing if replies start looking truncated under load.
    """

    def __init__(self, port: str, *, timeout: float = 2.0, baudrate: int = 115200) -> None:
        self.port = port
        self._serial = serial.Serial(
            port, baudrate=baudrate, timeout=timeout, write_timeout=timeout
        )

    def close(self) -> None:
        self._serial.close()

    def __enter__(self) -> Board:
        return self

    def __exit__(
        self,
        exc_type: type[BaseException] | None,
        exc: BaseException | None,
        tb: TracebackType | None,
    ) -> None:
        self.close()

    # -- raw line I/O ----------------------------------------------------------
    def send_line(self, text: str) -> None:
        self._serial.write(text.encode("ascii") + b"\n")
        self._serial.flush()

    def read_line(self) -> str | None:
        """Read one line within the port's timeout, or None if none arrived."""
        raw = self._serial.read_until(b"\n")
        if not raw:
            return None
        return raw.decode("ascii", errors="replace").rstrip("\r\n")

    def read_line_until(self, deadline: float) -> str | None:
        """Like read_line(), but keeps trying until `deadline` (time.monotonic())."""
        while True:
            line = self.read_line()
            if line is not None:
                return line
            if time.monotonic() >= deadline:
                return None

    def _drain_lines(self, sent: str) -> list[str]:
        """Collect reply lines until the board goes quiet, dropping the echo/prompt."""
        lines: list[str] = []
        while True:
            raw = self.read_line()
            if raw is None:
                return lines
            line = clean_line(raw)
            if line and not is_echo(line, sent):
                lines.append(line)

    # -- commands ----------------------------------------------------------------
    def command(self, text: str) -> str:
        """Send one text command and return its first reply line."""
        self.send_line(text)
        lines = self._drain_lines(text)
        if not lines:
            raise BoardError(f"{self.port}: no response to {text!r}")
        return lines[0]

    def version(self) -> str:
        return self.command("version")

    def radio_status(self) -> list[str]:
        """Send `radio status` and return all of its reply lines."""
        self.send_line("radio status")
        return self._drain_lines("radio status")

    def is_radio_ready(self) -> bool:
        return is_radio_ready(self.radio_status())

    def radio_ping(self, payload: str = "PING") -> str:
        """Send `radio ping <payload>`; returns the payload the board confirmed sending."""
        sent_command = f"radio ping {payload}"
        line = self.command(sent_command)
        sent = parse_ping_sent(line)
        if sent is None:
            raise BoardError(f"{self.port}: unexpected reply to {sent_command!r}: {line!r}")
        return sent

    def wait_for_rx(self, timeout: float = 5.0) -> RxFrame | None:
        """Wait up to `timeout` seconds for an unsolicited `radio rx: ...` line."""
        deadline = time.monotonic() + timeout
        while True:
            line = self.read_line_until(deadline)
            if line is None:
                return None
            frame = parse_rx_line(line)
            if frame is not None:
                return frame

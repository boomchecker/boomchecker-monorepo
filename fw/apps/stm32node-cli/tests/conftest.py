"""Shared test fixtures: an in-memory transport double (no hardware needed)."""

from __future__ import annotations

from stm32node_cli.transport.base import Transport


class FakeTransport(Transport):
    """In-memory Transport: reads from a preloaded buffer, records writes.

    This is a *test double*, not a runtime simulator - it just replays bytes we
    stage in the test so the protocol/client/session logic can be exercised
    without a real board.
    """

    def __init__(self, to_read: bytes = b"") -> None:
        self._rx = bytearray(to_read)
        self.written = bytearray()
        self.opened = False

    def open(self) -> None:
        self.opened = True

    def close(self) -> None:
        self.opened = False

    def write(self, data: bytes) -> None:
        self.written += data

    def read(self, size: int) -> bytes:
        chunk = bytes(self._rx[:size])
        del self._rx[:size]
        return chunk

    def feed(self, data: bytes) -> None:
        """Append more bytes to the read buffer."""
        self._rx += data

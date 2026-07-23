"""High-level device client: text commands and PCM streaming over a Transport."""

from __future__ import annotations

from collections.abc import Iterator
from dataclasses import dataclass

from ..transport.base import Transport
from .codec import ProtocolError, StreamHeader, encode_command, parse_header
from .spec import HEADER_SIZE, MAGIC


@dataclass
class StreamHandle:
    """A live PCM stream: its parsed header plus an iterator over payload chunks."""

    header: StreamHeader
    chunks: Iterator[bytes]

    def read_all(self) -> bytes:
        """Drain the stream into a single ``bytes`` object."""
        buf = bytearray()
        for chunk in self.chunks:
            buf += chunk
        return bytes(buf)


class DeviceClient:
    """Talks to the board over a :class:`Transport`.

    The transport is a byte pipe; this class owns the protocol semantics
    (command framing, header resync/parse, payload length handling).
    """

    def __init__(self, transport: Transport) -> None:
        self._t = transport

    # -- lifecycle -----------------------------------------------------------
    def open(self) -> None:
        self._t.open()

    def close(self) -> None:
        self._t.close()

    def __enter__(self) -> DeviceClient:
        self.open()
        return self

    def __exit__(self, *_exc: object) -> None:
        self.close()

    # -- commands ------------------------------------------------------------
    def version(self) -> str:
        """Send ``version`` and return the reported firmware version line."""
        self._t.write(encode_command("version"))
        return self._read_line().strip()

    def start_stream(
        self, seconds: int, *, source: str = "mic", chunk_size: int = 2048
    ) -> StreamHandle:
        """Send a stream command and return a handle to the incoming PCM stream.

        ``source="mic"`` streams the microphone (``stream``); ``source="test"``
        streams a synthetic tone (``streamtest``) for hardware-independent
        verification. Both use identical ``PCM1`` framing.

        The board may echo the command and print a prompt before the binary
        data, so we resync to the ``PCM1`` magic before parsing the header.
        """
        if seconds <= 0:
            raise ValueError("seconds must be positive")
        if source not in ("mic", "test"):
            raise ValueError("source must be 'mic' or 'test'")
        command = "streamtest" if source == "test" else "stream"
        self._t.write(encode_command(command, int(seconds)))
        self._resync_to_magic()
        rest = self._t.read_exact(HEADER_SIZE - len(MAGIC))
        header = parse_header(MAGIC + rest)
        return StreamHandle(header, self._iter_payload(header.byte_length, chunk_size))

    # -- internals -----------------------------------------------------------
    def _read_line(self) -> str:
        """Read bytes until a newline; returns the decoded line without it."""
        buf = bytearray()
        while True:
            b = self._t.read(1)
            if not b:
                # Transport timed out / closed; return what we have so far.
                break
            if b == b"\n":
                break
            if b != b"\r":
                buf += b
        return buf.decode("ascii", errors="replace")

    def _resync_to_magic(self) -> None:
        """Discard bytes until the 4-byte ``PCM1`` magic is seen."""
        window = bytearray()
        while True:
            b = self._t.read(1)
            if not b:
                raise ProtocolError("stream magic not found before transport timed out")
            window += b
            if len(window) > len(MAGIC):
                del window[0]
            if bytes(window) == MAGIC:
                return

    def _iter_payload(self, byte_length: int, chunk_size: int) -> Iterator[bytes]:
        remaining = byte_length
        while remaining > 0:
            n = min(chunk_size, remaining)
            chunk = self._t.read_exact(n)
            remaining -= len(chunk)
            yield chunk

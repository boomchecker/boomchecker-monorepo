"""Transport abstraction: the byte pipe the protocol runs over."""

from __future__ import annotations

from abc import ABC, abstractmethod


class TransportError(Exception):
    """Base class for transport failures."""


class TransportTimeout(TransportError):
    """Raised when the expected number of bytes did not arrive in time."""


class Transport(ABC):
    """A bidirectional byte pipe.

    Implementations only provide the primitives ``open``/``close``/``write``/
    ``read``; the shared :meth:`read_exact` and context-manager helpers are
    defined here so every transport (real or test) behaves the same.
    """

    @abstractmethod
    def open(self) -> None: ...

    @abstractmethod
    def close(self) -> None: ...

    @abstractmethod
    def write(self, data: bytes) -> None: ...

    @abstractmethod
    def read(self, size: int) -> bytes:
        """Read up to ``size`` bytes.

        May return fewer bytes than requested; returns ``b""`` on timeout or
        when the pipe is exhausted.
        """

    def read_exact(self, size: int) -> bytes:
        """Read exactly ``size`` bytes or raise :class:`TransportTimeout`."""
        buf = bytearray()
        while len(buf) < size:
            chunk = self.read(size - len(buf))
            if not chunk:
                raise TransportTimeout(f"expected {size} bytes, got {len(buf)}")
            buf += chunk
        return bytes(buf)

    def __enter__(self) -> Transport:
        self.open()
        return self

    def __exit__(self, *_exc: object) -> None:
        self.close()

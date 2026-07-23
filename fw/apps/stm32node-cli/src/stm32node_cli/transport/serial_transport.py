"""Real serial-port transport backed by pyserial."""

from __future__ import annotations

from dataclasses import dataclass

import serial
from serial.tools import list_ports as _list_ports

from .base import Transport, TransportError


@dataclass(frozen=True)
class PortInfo:
    """A discovered serial port."""

    device: str
    description: str


def list_ports() -> list[PortInfo]:
    """Enumerate available serial ports (for the port picker / `ports` command)."""
    return [PortInfo(p.device, p.description or "") for p in _list_ports.comports()]


class SerialTransport(Transport):
    """USB CDC ACM virtual COM port. Baud rate is irrelevant for CDC but pyserial
    requires a value, so a nominal default is used."""

    def __init__(self, port: str, *, timeout: float = 2.0, baudrate: int = 115200) -> None:
        self.port = port
        self.timeout = timeout
        self.baudrate = baudrate
        self._ser: serial.Serial | None = None

    def open(self) -> None:
        try:
            self._ser = serial.Serial(
                self.port,
                baudrate=self.baudrate,
                timeout=self.timeout,
                write_timeout=self.timeout,
            )
        except serial.SerialException as exc:
            raise TransportError(f"cannot open {self.port}: {exc}") from exc

    def close(self) -> None:
        if self._ser is not None:
            self._ser.close()
            self._ser = None

    def write(self, data: bytes) -> None:
        if self._ser is None:
            raise TransportError("transport not open")
        self._ser.write(data)
        self._ser.flush()

    def read(self, size: int) -> bytes:
        if self._ser is None:
            raise TransportError("transport not open")
        return self._ser.read(size)

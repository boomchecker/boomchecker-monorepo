"""Encode commands and decode the binary stream header, driven by ``spec``."""

from __future__ import annotations

from dataclasses import dataclass

from .spec import (
    HEADER_SIZE,
    HEADER_STRUCT,
    LINE_TERMINATOR,
    MAGIC,
    PROTOCOL_VERSION,
    SAMPLE_RATE_HZ,
    SAMPLE_WIDTH_BYTES,
    TRAILER_PREFIX,
)


class ProtocolError(Exception):
    """Raised when received bytes do not match the protocol spec."""


@dataclass(frozen=True)
class StreamHeader:
    """Parsed ``PCM1`` stream header."""

    version: int
    channels: int
    sample_rate: int
    byte_length: int

    @property
    def sample_count(self) -> int:
        return self.byte_length // SAMPLE_WIDTH_BYTES

    @property
    def duration_s(self) -> float:
        frames = self.sample_count / max(self.channels, 1)
        return frames / self.sample_rate if self.sample_rate else 0.0


@dataclass(frozen=True)
class StreamTrailer:
    """Parsed ``PCMEND`` end-of-stream trailer (capture health)."""

    overrun: bool  # acquisition dropped samples (gaps)
    err: bool  # source produced no data (payload was padded with silence)


def parse_trailer(line: str) -> StreamTrailer | None:
    """Parse a ``PCMEND overrun=0 err=0`` trailer line; None if it is not one."""
    text = line.strip()
    if not text.startswith(TRAILER_PREFIX.decode("ascii")):
        return None
    fields: dict[str, str] = {}
    for token in text.split()[1:]:
        key, _, value = token.partition("=")
        fields[key] = value
    return StreamTrailer(overrun=fields.get("overrun") == "1", err=fields.get("err") == "1")


def encode_command(name: str, *args: object) -> bytes:
    """Encode a text command line, e.g. ``encode_command("stream", 5)``."""
    parts = [name, *(str(a) for a in args)]
    return " ".join(parts).encode("ascii") + LINE_TERMINATOR


def pack_header(
    byte_length: int,
    *,
    channels: int = 1,
    sample_rate: int = SAMPLE_RATE_HZ,
    version: int = PROTOCOL_VERSION,
) -> bytes:
    """Build a ``PCM1`` header (used by tests and the future firmware model)."""
    return HEADER_STRUCT.pack(MAGIC, version, channels, 0, sample_rate, byte_length)


def parse_header(raw: bytes) -> StreamHeader:
    """Parse a 16-byte ``PCM1`` header, validating magic and length."""
    if len(raw) != HEADER_SIZE:
        raise ProtocolError(f"header must be {HEADER_SIZE} bytes, got {len(raw)}")
    magic, version, channels, _reserved, sample_rate, byte_length = HEADER_STRUCT.unpack(raw)
    if magic != MAGIC:
        raise ProtocolError(f"bad magic {magic!r}, expected {MAGIC!r}")
    return StreamHeader(
        version=version,
        channels=channels,
        sample_rate=sample_rate,
        byte_length=byte_length,
    )

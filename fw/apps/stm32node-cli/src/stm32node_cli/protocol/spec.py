"""Single source of truth for the host <-> STM32 serial protocol.

Everything about the wire format lives here as plain data so that the codec
(:mod:`stm32node_cli.protocol.codec`) and the documentation generator
(:mod:`stm32node_cli.protocol.gen_docs`) both derive from the same definitions.
When the protocol changes, edit this file and run ``task proto`` to refresh
``PROTOCOL.md`` (the contract the STM32 firmware must implement).
"""

from __future__ import annotations

import struct
from dataclasses import dataclass

# --- Audio format (matches firmware Core/Inc/pdm_pcm.h) ----------------------
SAMPLE_RATE_HZ = 48_000
CHANNELS = 1
SAMPLE_WIDTH_BYTES = 2  # int16, little-endian

# --- Framing -----------------------------------------------------------------
PROTOCOL_VERSION = 1
MAGIC = b"PCM1"
# Text commands are ASCII lines terminated by this byte.
LINE_TERMINATOR = b"\n"


@dataclass(frozen=True)
class HeaderField:
    """One field of the binary stream header."""

    name: str
    fmt: str  # struct format code, little-endian assumed by HEADER_STRUCT
    ctype: str  # matching C type, for the generated docs
    description: str


# Binary stream header, little-endian, 16 bytes total. Field order == byte order.
HEADER_FIELDS: tuple[HeaderField, ...] = (
    HeaderField("magic", "4s", "uint8_t[4]", 'Frame magic, always "PCM1".'),
    HeaderField("version", "B", "uint8_t", f"Protocol version (currently {PROTOCOL_VERSION})."),
    HeaderField("channels", "B", "uint8_t", "Channel count (1 = mono)."),
    HeaderField("reserved", "H", "uint16_t", "Reserved, must be 0."),
    HeaderField("sample_rate", "I", "uint32_t", f"Sample rate in Hz ({SAMPLE_RATE_HZ})."),
    HeaderField("byte_length", "I", "uint32_t", "Number of PCM payload bytes that follow."),
)

HEADER_STRUCT = struct.Struct("<" + "".join(f.fmt for f in HEADER_FIELDS))
HEADER_SIZE = HEADER_STRUCT.size  # 16


@dataclass(frozen=True)
class CommandSpec:
    """One text command the board understands on the command channel."""

    name: str
    usage: str
    description: str
    response: str


COMMANDS: tuple[CommandSpec, ...] = (
    CommandSpec(
        name="version",
        usage="version",
        description="Print the firmware version string.",
        response="A single text line, e.g. `bom-stm32node CLI v0.1`.",
    ),
    CommandSpec(
        name="stream",
        usage="stream <sec>",
        description="Stream <sec> seconds of PCM audio to the host.",
        response=(
            "A 16-byte `PCM1` header followed by exactly `byte_length` bytes of raw "
            "int16 little-endian PCM. After the payload the board returns to the text prompt."
        ),
    ),
)

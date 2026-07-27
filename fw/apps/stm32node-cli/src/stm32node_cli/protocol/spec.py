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
# The firmware acquires and streams whole half-buffers (PCM_SAMPLES_PER_HALF),
# so a transfer is rounded up to a whole number of these blocks.
SAMPLES_PER_BLOCK = 1024
# Longest stream the firmware accepts (Core/Inc/pcm_stream.h PCM_STREAM_MAX_SECONDS).
# The board rejects anything outside 1..this with a usage line, so the host must
# validate up front rather than mistaking that text for a missing acknowledgement.
STREAM_MAX_SECONDS = 60

# --- Framing -----------------------------------------------------------------
PROTOCOL_VERSION = 1
MAGIC = b"PCM1"
# Text commands are ASCII lines terminated by this byte.
LINE_TERMINATOR = b"\n"
# End-of-stream trailer sent after the payload, e.g. b"PCMEND overrun=0 err=0".
# Confirms the stream finished and reports capture health.
TRAILER_PREFIX = b"PCMEND"


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
    HeaderField(
        "byte_length",
        "I",
        "uint32_t",
        "Authoritative number of PCM payload bytes that follow "
        f"(a multiple of {SAMPLES_PER_BLOCK * SAMPLE_WIDTH_BYTES}).",
    ),
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
        description="Stream <sec> seconds of microphone PCM audio to the host.",
        response=(
            "A 16-byte `PCM1` header (the acknowledgement: parsing it confirms the command "
            "and gives `byte_length`), then exactly `byte_length` bytes of raw int16 "
            "little-endian PCM, then a `PCMEND` trailer line. The board pads silence if the "
            "microphone underruns, so the payload length is always honoured."
        ),
    ),
    CommandSpec(
        name="streamtest",
        usage="streamtest <sec>",
        description=(
            "Diagnostic: stream <sec> seconds of a synthetic 1 kHz test tone instead of the "
            "microphone. Same PCM1 framing as `stream`; lets the host verify enumeration, "
            "framing and decoding without depending on the mic hardware."
        ),
        response="Identical framing to `stream` (16-byte `PCM1` header + `byte_length` bytes).",
    ),
)

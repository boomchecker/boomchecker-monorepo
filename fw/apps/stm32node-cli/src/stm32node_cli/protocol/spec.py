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
    CommandSpec(
        name="detect",
        usage="detect <sec> [squelch_milli] [thr_milli] [dbg]",
        description=(
            "Run on-device drone detection for <sec> seconds (1..60): microphone PCM is "
            "decimated to 16 kHz, MFCC features are extracted (1024-sample frames, hop 512), "
            "every run of 14 frames above the RMS squelch is aggregated to a 52-value feature "
            "vector and classified by the model compiled into the firmware. That is currently "
            "a small MLP (v6), whose decision value is a raw logit, not a probability. "
            "Optional overrides in units of 1/1000: squelch_milli (default 10 = RMS 0.010, "
            "0 disables the gate, 0..1000) and thr_milli (default 7250 = logit 7.25, may be "
            "negative, -20000..20000 - a value outside that range is rejected, not clamped). "
            "A non-zero dbg adds one debug line per frame."
        ),
        response=(
            "A `LVL t=<s>.<ms> rms=<+d.ddd>` input-level line about once a second, one line "
            "per classified window: `DET t=<s>.<ms> dec=<+d.ddd> <DRONE|noise>` (windows are "
            "~448 ms of audio; input below the squelch yields no windows), then a final "
            "`DETEND windows=<n> drones=<n> overrun=<0|1> err=<0|1>` line. With dbg set, each "
            "frame also emits `F=<frame> a=<accumulated> r=<rms_milli> h=<half_us> "
            "m=<mfcc_us>`. A start failure prints `DETERR <reason>` and then the DETEND "
            "trailer with err=1, so the trailer always arrives."
        ),
    ),
    CommandSpec(
        name="gps",
        usage="gps <sec> [baud]",
        description=(
            "Stream raw NMEA sentences from the on-board Teseo-LIV3R GNSS module for <sec> "
            "seconds (1..300). The board re-inits UART4 at [baud] (default 9600, the "
            "module's ROM default; 1200..921600) and forwards each received line verbatim. "
            "The Teseo-LIV3R is a ROM part - its configuration does not persist without "
            "VBAT, so hosts should adapt to 9600 rather than reconfigure the module."
        ),
        response=(
            "A `GPS baud=<baud> sec=<sec>` acknowledgement line, then raw NMEA lines "
            "(`$G...*hh`, `$PSTM...*hh`) as they arrive, then a final `GPSEND lines=<n> "
            "bytes=<n> ne=<n> fe=<n> ore=<n> pe=<n> overrun=<n> err=<0|1>` trailer. The "
            "per-flag UART error counters separate marginal signal levels (ne, noise) "
            "from a wrong baud rate (fe, framing) and IRQ starvation (ore); err=1 means "
            "the host disconnected mid-run. A UART init failure prints `GPSERR <reason>` "
            "and then the GPSEND trailer with err=1, so the trailer always arrives."
        ),
    ),
    CommandSpec(
        name="gpstx",
        usage="gpstx <sentence> [baud]",
        description=(
            "Send one NMEA sentence to the GNSS module (e.g. `gpstx $PSTMGETSWVER`). The "
            "leading `$` is optional; the NMEA checksum and CRLF are appended by the board. "
            "The sentence must not contain spaces. UART reception stays armed afterwards, "
            "so the module's reply is buffered and delivered by the next `gps` run."
        ),
        response="`GPSTX ok` on success, `GPSERR tx failed` or a usage line otherwise.",
    ),
    CommandSpec(
        name="micdiag",
        usage="micdiag",
        description=(
            "PDM microphone wiring diagnostics. With the PDM clock running, samples the "
            "PDM_D1 (PE6) and PDM_D2 (PE4) data pins directly as GPIO inputs and counts "
            "level transitions (a transmitting mic toggles constantly; the count is "
            "qualitative). Then, with the clock stopped, a pull-up/pull-down test tells a "
            "floating/tri-stated line apart from one driven or shorted."
        ),
        response=(
            "Four `MICDIAG <pin> ...` lines (toggle counts with clk=on, pull test with "
            "clk=off) and a `MICDIAGEND err=<0|1>` trailer. A start failure prints the "
            "reason and the trailer with err=1, so the trailer always arrives."
        ),
    ),
    CommandSpec(
        name="gpsrst",
        usage="gpsrst",
        description=(
            "Pulse the GNSS module's SYS_RSTn line low for 100 ms (hardware restart of the "
            "Teseo-LIV3R). Bring-up fallback for a module that stays silent on every baud "
            "rate; the module cold-starts afterwards (RTC/time is lost without VBAT)."
        ),
        response="A single `GPSRST done (SYS_RSTn pulsed 100 ms)` line.",
    ),
    CommandSpec(
        name="dfu",
        usage="dfu",
        description=(
            "Reboot into the STM32 ROM bootloader for USB DFU flashing over this same USB "
            "port (no ST-Link needed). Flash with `STM32_Programmer_CLI -c port=USB1 -w "
            "<elf> -v` and power-cycle/reset to return to the application."
        ),
        response=(
            "A single `DFU: rebooting into the ROM bootloader` line, after which the CDC "
            "port disappears and the device re-enumerates as 'STM32 BOOTLOADER'."
        ),
    ),
)

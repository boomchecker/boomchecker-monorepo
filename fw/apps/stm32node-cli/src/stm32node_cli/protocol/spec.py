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

# --- On-device detector (matches firmware App/detect/detect_service.h, cli.c and
# the model tables in fw/common/boomdetect/models/) -------------------------------
# Defaults the board applies when `detect` is given fewer arguments. The squelch
# is the detector's; the threshold belongs to the model `model` last selected
# (thr_milli is a raw logit for the v6 MLP; 15.0 was set on hardware against room
# noise), so tests/test_firmware_defaults.py checks both against the firmware.
DETECT_MAX_SECONDS = 60
DETECT_DEFAULT_SQUELCH_MILLI = 10
DETECT_SQUELCH_MILLI_MAX = 1000
DETECT_MLP_V6_DEFAULT_THR_MILLI = 15000  # default_thr_milli of model_mlp_v6.c
DETECT_THR_MILLI_LIMIT = 20000  # accepted thr_milli range is -LIMIT..+LIMIT
# The K-of-N alarm above the classifier (App/detect/detect_service.h): ON when at
# least K_ON of the last N classified windows were called drone, OFF when fewer
# than K_OFF were. One window is ~448 ms; an alarm is a property of seconds.
DETECT_ALARM_N = 4
DETECT_ALARM_K_ON = 2
DETECT_ALARM_K_OFF = 1

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
            "0 disables the gate, 0..1000) and thr_milli (defaults to the selected "
            "model's own operating point, 15000 = logit 15.0 for mlp_v6, may be "
            "negative, -20000..20000 - a value outside that range is rejected, not clamped; "
            "the default was measured against ambient room noise on hardware, with no "
            "drone present, so it trades away an unquantified amount of sensitivity to "
            "avoid false alarms). "
            "A non-zero dbg adds one debug line per frame."
        ),
        response=(
            "A `LVL t=<s>.<ms> rms=<+d.ddd>` input-level line about once a second, one line "
            "per classified window: `DET t=<s>.<ms> span=<frames> dec=<+d.ddd> <DRONE|noise>` "
            "- `t` is when the window CLOSED and `span` how many frames it covered, which "
            "is not a constant: the RMS gate resets accumulation, so a window can straddle "
            "silence and start arbitrarily far from where the decision was made (windows are "
            "~448 ms of audio at the default hop; input below the squelch yields no windows). "
            "An `ALM t=<s>.<ms> <ON|OFF> hits=<k>/<n>` line whenever the K-of-N alarm changes "
            "state: ON once at least "
            f"{DETECT_ALARM_K_ON} of the last {DETECT_ALARM_N} classified windows were DRONE, "
            f"OFF once fewer than {DETECT_ALARM_K_OFF} were - printed only on transitions, so "
            "a steady drone gives one ON and one OFF, and a lone DRONE window gives nothing. "
            "Then a final "
            "`DETEND windows=<n> drones=<n> alarms=<n> overrun=<0|1> err=<0|1>` line, where "
            "alarms counts OFF-to-ON transitions. With dbg set, each "
            "frame also emits `F=<frame> a=<accumulated> r=<rms_milli> h=<half_us> "
            "m=<mfcc_us>`. A start failure prints `DETERR <reason>` and then the DETEND "
            "trailer with err=1, so the trailer always arrives."
        ),
    ),
    CommandSpec(
        name="model",
        usage="model [name]",
        description=(
            "List the classifiers compiled into this image, or select one for subsequent "
            "`detect` runs. With no argument it prints one line per model, marking the "
            "active one with `*` and showing the feature range it reads and its own "
            "default threshold. The selection is not persisted; a reset returns to the "
            "deployed model."
        ),
        response=(
            "`model: <name> <*| > feat=<lo>..<hi> thr=<milli>` per model when listing - the "
            "name is left-padded to 8 columns, so a parser must strip whitespace rather than "
            "split on a single space - or "
            "`model: <name> selected, default thr=<milli> (not persisted)` when selecting. "
            "`model: no such model '<name>'` otherwise."
        ),
    ),
    CommandSpec(
        name="detselftest",
        usage="detselftest",
        description=(
            "Drive the whole detection chain from a fixed synthetic signal and print every "
            "stage as raw IEEE-754 bit patterns. The input is an integer LCG "
            "(`s = s*1103515245 + 12345`, seed 1, sample `(int16)(((s>>16)&0xFFFF)-32768)/4`), "
            "so it is bit-identical on any platform and costs no flash. It exists because "
            "the microphone never repeats an input, so two `detect` runs can never be "
            "compared; this one can, and a mismatch against "
            "`fw/common/boomdetect/tests/vectors/selftest_expected.txt` means the arithmetic "
            "moved. Always runs the `mlp_v6` model, whatever `model` last selected."
        ),
        response=(
            "A leading blank line and `DSTBEGIN`, then `DSTMFCC f=<frame> <13 hex words>` for "
            "the first three frames, `DSTFEAT w=<window> <mean|std |dmea|cmax> <13 hex words>` "
            "for each window, `DSTDEC w=<window> logit=<8 hex digits>`, a "
            "`DSTSIG n=<samples> seed=<n> fnv=<8 hex digits>` line covering the generated "
            "input, and a final `DSTEND frames=<n> windows=<n> err=<0|1>` trailer. Every "
            "float is its raw bit pattern, not a decimal, so \"unchanged\" means unchanged. "
            "If the model is missing or the detector fails to init, `DSTERR <reason>` "
            "precedes the trailer with err=1."
        ),
    ),
    CommandSpec(
        name="micslot",
        usage="micslot [a|b]",
        description=(
            "Show or select which microphone of the PDM pair the DSP demodulates. The two "
            "mics of a pair share one 16-bit SAI word, split by a bit mask (A = 0xF807, "
            "B = 0x07F8); which one is populated is a board-build property. With the wrong "
            "slot the chain decodes the empty half of every frame and reports a flat zero, "
            "which is indistinguishable from a perfectly quiet detector, so `micdiag` is the "
            "way to tell them apart. The selection is a bring-up override and is not "
            "persisted; a reset returns to the firmware default. Takes effect on the next "
            "`detect` or `stream`."
        ),
        response=(
            "`micslot: <A|B|custom> (0x<mask>)` when showing, "
            "`micslot: <A|B> selected (next detect/stream)` when selecting, "
            "`usage: micslot [a|b]` otherwise."
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
            "VBAT, so hosts should adapt to 9600 rather than reconfigure the module. UART "
            "reception is switched off again when the run ends, so no stale sentences pile "
            "up between commands; a reply buffered by an earlier `gpstx` is delivered first."
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
            "The sentence must not contain spaces. The board discards any stale input, arms "
            "UART reception and then transmits, so the module's reply (plus roughly the next "
            "second of NMEA, after which the 1 KB buffer drops further bytes) is buffered and "
            "delivered at the start of the next `gps` run. Several `gpstx` in a row queue "
            "their replies behind each other until a `gps` drains them."
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

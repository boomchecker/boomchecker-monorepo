"""Pure parsing for bom-stm32node's radio bring-up CLI commands (cli.c).

Covers `radio status`, `radio ping` and the unsolicited `radio rx: ...` line
cli.c prints for every received packet (see boomlink.md section 15.3's "raw
RadioLib ping/pong" hardware test). No serial I/O here - that is board.py's
job - so these are testable without a physical board (tests/test_protocol.py).
"""

from __future__ import annotations

import re
from dataclasses import dataclass

# embedded-cli echoes every received character wrapped in cursor save/restore
# escapes, e.g. b"\x1b[s\x1b[u" (same quirk stm32node_cli's DeviceClient works
# around for the stream/version commands).
_ANSI_RE = re.compile(r"\x1b\[[0-9;?]*[ -/]*[@-~]")

# Mirrors cli.c's print_rx_frame(): 'radio rx: "%s"%s (%u bytes, RSSI %s dBm, SNR %s dB)'.
_RX_RE = re.compile(
    r'^radio rx: "(?P<text>.*)"(?P<truncated>\.\.\.)? '
    r"\((?P<byte_count>\d+) bytes, RSSI (?P<rssi>-?\d+\.\d+) dBm, SNR (?P<snr>-?\d+\.\d+) dB\)$"
)

# Mirrors cli.c's cmd_radio(): the "ping" subcommand's two possible replies.
_PING_SENT_RE = re.compile(r'^radio ping: sent "(?P<text>.*)" \((?P<byte_count>\d+) bytes\)$')
_PING_FAILED_RE = re.compile(r"^radio ping: failed \(error (?P<error>-?\d+)\)$")


@dataclass(frozen=True)
class RxFrame:
    """One parsed `radio rx: ...` line."""

    text: str
    truncated: bool
    byte_count: int
    rssi_dbm: float
    snr_db: float


class RadioPingError(RuntimeError):
    """The board's `radio ping` command itself reported a send failure."""


def clean_line(line: str) -> str:
    """Strip ANSI escapes and a leading `> ` prompt from one console line."""
    line = _ANSI_RE.sub("", line).strip()
    if line.startswith("> "):
        line = line[2:]
    return line.strip()


def is_echo(line: str, sent: str) -> bool:
    """True if `line` is embedded-cli echoing back the command we just sent."""
    return line == sent


def parse_rx_line(line: str) -> RxFrame | None:
    """Parse a `radio rx: "..." (N bytes, RSSI X dBm, SNR Y dB)` line, or None."""
    m = _RX_RE.match(clean_line(line))
    if m is None:
        return None
    return RxFrame(
        text=m["text"],
        truncated=m["truncated"] is not None,
        byte_count=int(m["byte_count"]),
        rssi_dbm=float(m["rssi"]),
        snr_db=float(m["snr"]),
    )


def parse_ping_sent(line: str) -> str | None:
    """Parse a successful `radio ping: sent "..." (N bytes)` line.

    Returns the payload the board echoed back, or None if `line` is neither
    reply form `radio ping` produces. Raises RadioPingError if it is the
    command's own reported send failure.
    """
    cleaned = clean_line(line)
    m = _PING_SENT_RE.match(cleaned)
    if m is not None:
        return m["text"]
    failed = _PING_FAILED_RE.match(cleaned)
    if failed is not None:
        raise RadioPingError(f"radio ping failed with error {failed['error']}")
    return None


def is_radio_ready(status_lines: list[str]) -> bool:
    """True if `radio status`'s first line reports the radio ready (vs "not ready")."""
    if not status_lines:
        return False
    return clean_line(status_lines[0]).startswith("radio: ready")

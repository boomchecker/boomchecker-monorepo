"""Application defaults."""

from __future__ import annotations

import sys
from pathlib import Path

# Sensible default CDC port per platform (overridable on the CLI / in the TUI).
if sys.platform.startswith("win"):
    DEFAULT_PORT = "COM4"
elif sys.platform == "darwin":
    DEFAULT_PORT = "/dev/tty.usbmodem1"
else:
    DEFAULT_PORT = "/dev/ttyACM0"

DEFAULT_BAUD = 115200
DEFAULT_TIMEOUT_S = 2.0


def default_output_dir() -> Path:
    """Where recordings are written by default: ``./recordings`` (gitignored)."""
    return Path.cwd() / "recordings"

#!/usr/bin/env python3
"""Compare a detselftest capture against the checked-in reference."""
import re
import subprocess
import sys
from pathlib import Path

REF = Path(__file__).resolve().parent / "selftest_expected.txt"


def expected():
    return [l for l in REF.read_text().splitlines() if l.startswith("DST")]


def captured(port="/dev/ttyACM1"):
    import serial  # noqa: PLC0415 - optional, only needed when talking to a board
    ansi = re.compile(r"\x1b\[[0-9;]*[A-Za-z]")
    with serial.Serial(port, 115200, timeout=0.3, write_timeout=5) as s:
        s.write(b"\n")
        s.reset_input_buffer()
        s.write(b"detselftest\n")
        s.flush()
        import time
        buf, end = b"", time.time() + 30
        while time.time() < end:
            buf += s.read(4096)
    text = ansi.sub("", buf.decode("utf-8", "replace"))
    # Tolerate the command echo running into the first line: match DST* wherever
    # it starts, not only at column zero.
    out = []
    for line in text.splitlines():
        i = line.find("DST")
        if i >= 0:
            out.append(line[i:].strip())
    return out


def main():
    got = captured(sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyACM1")
    want = expected()
    if got == want:
        print(f"MATCH: {len(want)} lines identical to {REF.name}")
        return 0
    print(f"MISMATCH ({len(got)} captured vs {len(want)} expected)")
    for i, (a, b) in enumerate(zip(want, got)):
        if a != b:
            print(f"  line {i}:\n    expected {a}\n    got      {b}")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())

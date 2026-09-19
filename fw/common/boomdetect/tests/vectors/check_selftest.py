#!/usr/bin/env python3
"""Compare a connected board's `detselftest` output against the recorded fixture.

The board's only real input is a live microphone, which never repeats, so two
`detect` runs can never be compared. This one can: `detselftest` drives the whole
chain from an integer LCG and prints every stage as raw IEEE-754 bit patterns.

This checks the BOARD against selftest_expected.txt. The host has its own
baseline (selftest_host.txt) checked by ctest, and the two do not match; see that
file's header for the measurement and why it is expected.
"""

import glob
import itertools
import re
import sys
import time
from pathlib import Path

REF = Path(__file__).resolve().parent / "selftest_expected.txt"

# The console, not the ST-Link's own VCP. Matched by id rather than by
# /dev/ttyACMn because the number moves between reflashes and one of the two
# nodes is the debugger; docs/firmware/bom-stm32node/build.md says the same.
PORT_GLOB = "/dev/serial/by-id/usb-STMicroelectronics_boomchecker-node_*-if00"

ANSI = re.compile(r"\x1b\[[0-9;]*[A-Za-z]")


def default_port():
    matches = sorted(glob.glob(PORT_GLOB))
    if not matches:
        raise SystemExit(
            f"no board found at {PORT_GLOB}\n"
            "Pass the device explicitly if it enumerates under another name."
        )
    if len(matches) > 1:
        raise SystemExit(
            "several boards match; pass one explicitly:\n  " + "\n  ".join(matches)
        )
    return matches[0]


def expected():
    return [line for line in REF.read_text().splitlines() if line.startswith("DST")]


def captured(port, timeout_s=30.0):
    import serial  # noqa: PLC0415 - only needed when talking to a board

    with serial.Serial(port, 115200, timeout=0.3, write_timeout=5) as s:
        s.write(b"\n")
        time.sleep(0.2)
        s.reset_input_buffer()
        s.write(b"detselftest\n")
        s.flush()
        buf, end = b"", time.time() + timeout_s
        # Stop at the trailer rather than always burning the full timeout: the
        # run takes well under a second and the terminator is unambiguous.
        while time.time() < end and b"DSTEND" not in buf:
            buf += s.read(4096)

    text = ANSI.sub("", buf.decode("utf-8", "replace"))
    # Tolerate the command echo running into the first line: match DST* wherever
    # it starts, not only at column zero.
    out = []
    for line in text.splitlines():
        i = line.find("DST")
        if i >= 0:
            out.append(line[i:].strip())
    return out


def signature(lines):
    for line in lines:
        if line.startswith("DSTSIG"):
            return line
    return None


def main():
    port = sys.argv[1] if len(sys.argv) > 1 else default_port()
    got = captured(port)
    want = expected()

    if not got:
        print(f"nothing captured from {port}; is the console attached elsewhere?")
        return 1

    # The signature first. It is the checksum of the generated INPUT, so if it
    # differs the two sides did not score the same signal and every downstream
    # difference is a consequence rather than a finding of its own.
    sig_want, sig_got = signature(want), signature(got)
    if sig_got is None:
        print("no DSTSIG line captured; the run was cut short")
        return 1
    if sig_want != sig_got:
        print("input signature differs, so nothing downstream is comparable:")
        print(f"    expected {sig_want}")
        print(f"    got      {sig_got}")
        return 1

    if got == want:
        print(f"MATCH: {len(want)} lines identical to {REF.name}")
        return 0

    print(f"MISMATCH ({len(got)} captured vs {len(want)} expected)")
    # zip_longest, not zip: a capture that dies halfway would otherwise be
    # truncated to the shorter list and print nothing but the count line.
    for i, (a, b) in enumerate(itertools.zip_longest(want, got)):
        if a != b:
            print(f"  line {i}:")
            print(f"    expected {a if a is not None else '<missing>'}")
            print(f"    got      {b if b is not None else '<missing>'}")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Session helper for the boomchecker-node board (STM32H563, USB CDC console).

Solves the two things that burned time on 2026-08-07:
  * opening the COM port during re-enumeration creates a zombie handle that
    bricks the port until the next enumeration -> every open here waits for
    the port to be listed, sleeps a settle delay, then opens with retries and
    confirms with a `version` handshake;
  * the DFU reflash cycle (`dfu` -> STM32_Programmer_CLI -> physical RESET)
    had no automation -> `dfu-flash` drives the whole cycle and only pauses
    for the one thing software cannot do: pressing the black RESET button.

Run with the stm32node-cli venv (has pyserial):
  fw/apps/stm32node-cli/.venv/Scripts/python fw/bom-stmnode/drony/tools/board_session.py <cmd>

Subcommands:
  find                      print the board's COM port (VID:PID 0483:5710) or exit 1
  wait                      wait for the port to (re)appear, settle, handshake `version`
  cmd "<line>"              send one CLI line, stream output until trailer/idle
  detect <sec> [opts]       run `detect`, live-print DET/LVL lines, summarize h=/m= stats
  dfu-flash [--elf PATH]    handshake -> `dfu` -> flash over DFU -> wait for RESET press
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path

import serial
from serial.tools import list_ports

VID, PID = 0x0483, 0x5710
BAUD = 115200  # irrelevant for CDC, pyserial needs a value
SETTLE_S = 2.0  # after the port is listed, before opening (zombie-handle guard)
FLASHER = r"C:\Programy\PG\bin\STM32_Programmer_CLI.exe"
REPO_ROOT = Path(__file__).resolve().parents[4]
DEFAULT_ELF = REPO_ROOT / "fw" / "bom-stm32node" / "build" / "Debug" / "bom-stm32node.elf"
LOG_DIR = Path(__file__).resolve().parents[1] / "data" / "detect_logs"

# embedded-cli wraps the echo in cursor save/restore (ESC7/ESC8) + CSI sequences
ANSI_RE = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]|\x1b[78]")


def log(msg: str) -> None:
    print(msg, flush=True)


def find_port() -> str | None:
    for p in list_ports.comports():
        if p.vid == VID and p.pid == PID:
            return p.device
    return None


def wait_for_port(timeout: float, *, want_absent: bool = False) -> str | None:
    """Poll until the board's CDC port appears (or disappears with want_absent)."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        port = find_port()
        if want_absent and port is None:
            return None
        if not want_absent and port is not None:
            return port
        time.sleep(0.5)
    raise TimeoutError(
        f"port did not {'disappear' if want_absent else 'appear'} within {timeout:.0f}s"
    )


def clean(line: str) -> str:
    line = ANSI_RE.sub("", line)
    while line.startswith("> "):
        line = line[2:]
    return line.strip()


def open_port(port: str, *, settle: float = SETTLE_S) -> serial.Serial:
    """Open after a settle delay, with retries. Never call right after a reset
    without going through wait_for_port first."""
    time.sleep(settle)
    last: Exception | None = None
    for attempt in range(1, 4):
        try:
            return serial.Serial(port, BAUD, timeout=0.3, write_timeout=2.0)
        except serial.SerialException as exc:
            last = exc
            log(f"  open {port} failed (attempt {attempt}/3): {exc}")
            time.sleep(1.0)
    raise RuntimeError(f"cannot open {port}: {last}")


def handshake(ser: serial.Serial, *, retries: int = 5) -> str:
    """`version` with retries; returns the version line. Proves the console is alive."""
    for attempt in range(1, retries + 1):
        ser.reset_input_buffer()
        ser.write(b"\n")  # fresh prompt, flushes any half-typed junk
        ser.flush()
        time.sleep(0.2)
        ser.reset_input_buffer()
        ser.write(b"version\n")
        ser.flush()
        deadline = time.monotonic() + 2.0
        buf = b""
        while time.monotonic() < deadline:
            buf += ser.read(256)
            for raw in buf.decode("utf-8", "replace").splitlines():
                line = clean(raw)
                if "CLI v" in line:
                    return line
        log(f"  handshake attempt {attempt}/{retries}: no version reply")
        time.sleep(0.5)
    raise RuntimeError("handshake failed: board did not answer `version`")


def connect(timeout: float = 60.0, *, settle: float = SETTLE_S) -> tuple[serial.Serial, str]:
    log(f"waiting for boomchecker-node (VID:PID {VID:04x}:{PID:04x}) ...")
    port = wait_for_port(timeout)
    log(f"  port listed: {port}; settling {settle:.1f}s before open")
    ser = open_port(port, settle=settle)
    version = handshake(ser)
    log(f"  handshake OK on {port}: {version}")
    return ser, version


def stream_command(
    ser: serial.Serial,
    cmdline: str,
    *,
    timeout: float,
    until_prefixes: tuple[str, ...] = ("DETEND", "PCMEND", "DETERR"),
    idle_stop: float | None = None,
    logfile: Path | None = None,
) -> list[str]:
    """Send one CLI line and live-print cleaned output lines until a trailer
    prefix, an idle period (if idle_stop is set), or the deadline."""
    sent = cmdline.strip()
    ser.reset_input_buffer()
    ser.write(sent.encode("ascii") + b"\n")
    ser.flush()
    lines: list[str] = []
    buf = b""
    deadline = time.monotonic() + timeout
    last_data = time.monotonic()
    fh = logfile.open("w", encoding="utf-8") if logfile else None
    try:
        while time.monotonic() < deadline:
            chunk = ser.read(4096)
            if chunk:
                last_data = time.monotonic()
                buf += chunk
                while b"\n" in buf:
                    raw, buf = buf.split(b"\n", 1)
                    line = clean(raw.decode("utf-8", "replace"))
                    if not line or line == sent:
                        continue
                    lines.append(line)
                    log(f"  | {line}")
                    if fh:
                        fh.write(line + "\n")
                    if any(line.startswith(p) for p in until_prefixes):
                        return lines
            elif idle_stop and lines and time.monotonic() - last_data > idle_stop:
                return lines
        log(f"  ! deadline {timeout:.0f}s hit without trailer {until_prefixes}")
        return lines
    finally:
        if fh:
            fh.close()


BREADCRUMB_RE = re.compile(r"^F=(\d+) a=(\d+) r=(\d+) h=(\d+) m=(\d+)$")


def summarize_detect(lines: list[str]) -> None:
    h = [int(m.group(4)) for line in lines if (m := BREADCRUMB_RE.match(line))]
    m_ = [int(m.group(5)) for line in lines if (m := BREADCRUMB_RE.match(line))]
    det = [line for line in lines if line.startswith("DET ")]
    end = [line for line in lines if line.startswith("DETEND")]
    log("--- detect summary ---")
    if h:
        # budget per 48 kHz half is 21333 us; h includes the ~17 ms PDM->PCM DSP
        log(f"  h (mic_poll+PDM) us: max={max(h)} avg={sum(h) // len(h)} n={len(h)}")
    if m_nonzero := [v for v in m_ if v]:
        log(
            f"  m (MFCC) us:         max={max(m_nonzero)} "
            f"avg={sum(m_nonzero) // len(m_nonzero)} n={len(m_nonzero)}"
        )
    if h and m_nonzero:
        worst = max(h) + max(m_nonzero)
        log(f"  worst-case h+m = {worst} us vs 21333 us half budget")
    for line in det:
        log(f"  {line}")
    log(f"  {end[0]}" if end else "  ! no DETEND trailer (wedge? deadline?)")


def cmd_find(_: argparse.Namespace) -> int:
    port = find_port()
    if port:
        log(port)
        return 0
    log("boomchecker-node not enumerated")
    return 1


def cmd_wait(args: argparse.Namespace) -> int:
    ser, _ = connect(args.timeout, settle=args.settle)
    ser.close()
    return 0


def cmd_cmd(args: argparse.Namespace) -> int:
    ser, _ = connect(args.timeout_connect, settle=args.settle)
    try:
        stream_command(ser, args.line, timeout=args.timeout, idle_stop=2.0)
    finally:
        ser.close()
    return 0


def cmd_detect(args: argparse.Namespace) -> int:
    ser, _ = connect(args.timeout_connect, settle=args.settle)
    LOG_DIR.mkdir(parents=True, exist_ok=True)
    logfile = LOG_DIR / f"detect_{datetime.now():%Y%m%d_%H%M%S}.txt"
    line = f"detect {args.sec} {args.squelch} {args.thr} {1 if args.dbg else 0}"
    log(f"running `{line}` (log: {logfile})")
    try:
        # generous margin: blocked USB writes may stretch the run past <sec>,
        # and closing mid-run wedges the console until the run ends (never
        # abandon a running detect - see PROGRESS_REPORT 2026-08 section 6.3)
        lines = stream_command(ser, line, timeout=args.sec + 45, logfile=logfile)
    finally:
        ser.close()
    summarize_detect(lines)
    return 0


def cmd_dfu_flash(args: argparse.Namespace) -> int:
    elf = Path(args.elf)
    if not elf.exists():
        log(f"ELF not found: {elf}")
        return 1
    ser, _ = connect(args.timeout_connect, settle=args.settle)
    log("sending `dfu` (board will drop to the ROM bootloader) ...")
    stream_command(ser, "dfu", timeout=4.0, until_prefixes=("DFU:",), idle_stop=1.5)
    ser.close()
    log("waiting for the CDC port to disappear ...")
    wait_for_port(20.0, want_absent=True)
    time.sleep(2.5)  # bootloader USB enumeration
    flash_cmd = [args.flasher, "-c", "port=USB1", "-w", str(elf), "-v"]
    log(f"flashing: {' '.join(flash_cmd)}")
    for attempt in range(1, 4):
        proc = subprocess.run(flash_cmd, capture_output=True, text=True)
        out = proc.stdout + proc.stderr
        ok = proc.returncode == 0 and (
            "verified successfully" in out or "File download complete" in out
        )
        if ok:
            log("  flash + verify OK")
            break
        tail = "\n".join(out.strip().splitlines()[-6:])
        log(f"  flash attempt {attempt}/3 failed (rc={proc.returncode}):\n{tail}")
        if attempt == 3:
            return 1
        time.sleep(2.0)
    log("")
    log("=== TED STISKNI CERNE RESET TLACITKO NA DESCE ===")
    log("(po DFU flashi je fyzicky reset povinny; -g nechava USB aplikace mrtve)")
    log("")
    ser, version = connect(args.timeout_reset, settle=args.settle)
    ser.close()
    log(f"board is back: {version} — flash cycle complete")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--settle", type=float, default=SETTLE_S, help="seconds between port listing and open")
    sub = ap.add_subparsers(dest="sub", required=True)

    sub.add_parser("find").set_defaults(fn=cmd_find)

    p = sub.add_parser("wait")
    p.add_argument("--timeout", type=float, default=120.0)
    p.set_defaults(fn=cmd_wait)

    p = sub.add_parser("cmd")
    p.add_argument("line", help='CLI line to send, e.g. "detect 8 3 250 1"')
    p.add_argument("--timeout", type=float, default=30.0)
    p.add_argument("--timeout-connect", type=float, default=30.0)
    p.set_defaults(fn=cmd_cmd)

    p = sub.add_parser("detect")
    p.add_argument("sec", type=int)
    p.add_argument("--squelch", type=int, default=10, help="RMS gate in milli (10 = 0.010)")
    p.add_argument("--thr", type=int, default=7250,
                   help="decision threshold in milli (v6 champion point: 7250 = logit 7.25)")
    p.add_argument("--dbg", action="store_true", help="per-frame F=/a=/r=/h=/m= breadcrumbs")
    p.add_argument("--timeout-connect", type=float, default=30.0)
    p.set_defaults(fn=cmd_detect)

    p = sub.add_parser("dfu-flash")
    p.add_argument("--elf", default=str(DEFAULT_ELF))
    p.add_argument("--flasher", default=FLASHER)
    p.add_argument("--timeout-connect", type=float, default=30.0)
    p.add_argument("--timeout-reset", type=float, default=300.0, help="how long to wait for the RESET press")
    p.set_defaults(fn=cmd_dfu_flash)

    args = ap.parse_args()
    try:
        return args.fn(args)
    except (TimeoutError, RuntimeError) as exc:
        log(f"ERROR: {exc}")
        return 1


if __name__ == "__main__":
    sys.exit(main())

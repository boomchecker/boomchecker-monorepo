#!/usr/bin/env python3
"""Teseo-LIV3R GNSS bring-up helper for the boomchecker-node board.

Talks to the firmware's `gps`/`gpstx` console commands (see fw gps.c): the
board streams raw NMEA lines over USB CDC and ends with a `GPSEND` trailer.
This script logs the raw sentences, parses GGA/RMC/GSV on the fly and prints
a per-second status line (fix quality, satellites, HDOP, position).

Run with a venv that has pyserial:
  fw/apps/stm32node-cli/.venv/Scripts/python fw/bom-stmnode/drony/tools/gps_log.py <cmd>

Subcommands:
  capture <sec> [--baud 9600]   stream, log to data/gps_logs/*.nmea, parse live
  scan                          try 9600/115200/38400/57600/4800, report which talks
  ver                           gpstx $PSTMGETSWVER + short capture, print $PSTMVER
"""

from __future__ import annotations

import argparse
import sys
import time
from datetime import datetime
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from board_session import clean, connect, log  # noqa: E402

GPS_LOG_DIR = Path(__file__).resolve().parents[1] / "data" / "gps_logs"
SCAN_BAUDS = (9600, 115200, 38400, 57600, 4800)


# --- NMEA parsing (stdlib only; first parser in the repo) -------------------

def nmea_checksum_ok(line: str) -> bool:
    """`$BODY*HH` -> XOR of BODY chars equals HH."""
    if not line.startswith("$") or "*" not in line:
        return False
    body, _, tail = line[1:].partition("*")
    if len(tail) < 2:
        return False
    try:
        want = int(tail[:2], 16)
    except ValueError:
        return False
    csum = 0
    for ch in body:
        csum ^= ord(ch)
    return csum == want


def _coord(value: str, hemi: str, degdigits: int) -> float | None:
    """ddmm.mmmm / dddmm.mmmm + N/S/E/W -> signed decimal degrees."""
    if not value or not hemi:
        return None
    try:
        deg = int(value[:degdigits])
        minutes = float(value[degdigits:])
    except ValueError:
        return None
    dec = deg + minutes / 60.0
    return -dec if hemi in ("S", "W") else dec


TALKER_SYS = {"GP": "GPS", "GL": "GLONASS", "GA": "Galileo", "GB": "BeiDou",
              "BD": "BeiDou", "GQ": "QZSS", "GN": "multi"}
FIX_TYPE = {1: "no", 2: "2D", 3: "3D"}


class GpsState:
    """Rolling summary built from the parsed sentences."""

    def __init__(self) -> None:
        self.sentences = 0
        self.cksum_ok = 0
        self.types: dict[str, int] = {}
        self.utc = ""
        self.date = ""
        self.fix_quality = 0
        self.fix_type = 1  # GSA: 1=no, 2=2D, 3=3D
        self.nsats_used = 0
        self.hdop = ""
        self.pdop = ""
        self.vdop = ""
        self.alt = ""
        self.lat: float | None = None
        self.lon: float | None = None
        self.sats_in_view: dict[str, int] = {}  # per talker (GP/GL/GB/...)
        # per-satellite link quality from GSV: prn -> (system, elev, az, snr, seen_at)
        self.sats: dict[int, tuple[str, str, str, str, float]] = {}
        self._gsa_hist: list[tuple[float, frozenset[int]]] = []
        self.positions: list[tuple[float, float]] = []  # fixes for scatter stats
        self.pstm: list[str] = []

    def used_prns(self) -> frozenset[int]:
        """PRNs listed in a GSA sentence within the last ~2.5 s."""
        now = time.monotonic()
        out: set[int] = set()
        for t, prns in self._gsa_hist:
            if now - t < 2.5:
                out |= prns
        return frozenset(out)

    def update(self, line: str) -> None:
        if not line.startswith("$"):
            return
        self.sentences += 1
        ok = nmea_checksum_ok(line)
        if ok:
            self.cksum_ok += 1
        body = line[1:].partition("*")[0]
        fields = body.split(",")
        head = fields[0]  # e.g. GPGGA / GNRMC / PSTMVER
        self.types[head] = self.types.get(head, 0) + 1
        if head.startswith("PSTM"):
            if head != "PSTMCPU":  # periodic CPU telemetry, not a reply
                self.pstm.append(line)
            return
        if not ok or len(head) != 5:
            return
        talker, kind = head[:2], head[2:]
        if kind == "GGA" and len(fields) >= 10:
            self.utc = fields[1]
            lat = _coord(fields[2], fields[3], 2)
            lon = _coord(fields[4], fields[5], 3)
            if lat is not None and lon is not None:
                self.lat, self.lon = lat, lon
            self.fix_quality = int(fields[6] or 0)
            self.nsats_used = int(fields[7] or 0)
            self.hdop = fields[8]
            self.alt = fields[9]
            if self.fix_quality > 0 and lat is not None and lon is not None:
                self.positions.append((lat, lon))
        elif kind == "RMC" and len(fields) >= 10:
            self.utc = fields[1] or self.utc
            self.date = fields[9] or self.date
        elif kind == "GSV" and len(fields) >= 4:
            try:
                self.sats_in_view[talker] = int(fields[3])
            except ValueError:
                pass
            system = TALKER_SYS.get(talker, talker)
            now = time.monotonic()
            for i in range(4, len(fields) - 2, 4):
                prn, elev, az = fields[i], fields[i + 1], fields[i + 2]
                snr = fields[i + 3] if i + 3 < len(fields) else ""
                if prn:
                    try:
                        nprn = int(prn)
                    except ValueError:
                        continue
                    # NMEA quirk: 33..64 under a GP talker are SBAS (PRN+87),
                    # e.g. 36 = EGNOS PRN 123.
                    sys_ = f"SBAS({nprn + 87})" if 33 <= nprn <= 64 else system
                    self.sats[nprn] = (sys_, elev, az, snr, now)
        elif kind == "GSA" and len(fields) >= 18:
            try:
                self.fix_type = int(fields[2] or 1)
            except ValueError:
                pass
            prns = frozenset(int(p) for p in fields[3:15] if p.isdigit())
            self._gsa_hist.append((time.monotonic(), prns))
            self._gsa_hist = self._gsa_hist[-8:]
            self.pdop, self.vdop = fields[15], fields[17]

    def status_line(self) -> str:
        pos = "no-fix"
        if self.fix_quality > 0 and self.lat is not None:
            pos = f"{self.lat:+.5f},{self.lon:+.5f} alt={self.alt}m"
        view = "+".join(f"{t}:{n}" for t, n in sorted(self.sats_in_view.items()))
        pct = 100.0 * self.cksum_ok / self.sentences if self.sentences else 0.0
        return (
            f"sent={self.sentences} cks={pct:.0f}% "
            f"fix={self.fix_quality}/{FIX_TYPE.get(self.fix_type, '?')} "
            f"used={self.nsats_used} view=[{view}] hdop={self.hdop or '-'} "
            f"pdop={self.pdop or '-'} utc={self.utc or '-'} {pos}"
        )

    def sat_table(self) -> list[str]:
        """Per-satellite link quality, tracked satellites first (by C/N0)."""
        now = time.monotonic()
        used = self.used_prns()
        rows = [(prn, sys_, el, az, snr) for prn, (sys_, el, az, snr, seen)
                in self.sats.items() if now - seen < 6.0]
        if not rows:
            return ["  (no satellites reported in GSV)"]

        def key(r):  # tracked (has SNR) first, strongest first
            return (r[4] == "", -int(r[4] or 0), r[0])

        out = ["  PRN  system   used  elev  azim  C/N0"]
        for prn, sys_, el, az, snr in sorted(rows, key=key):
            out.append(
                f"  {prn:>3}  {sys_:<8} {'*' if prn in used else ' ':^4} "
                f"{el or '-':>4}  {az or '-':>4}  "
                f"{(snr + ' dB-Hz') if snr else 'searching'}"
            )
        return out

    def scatter(self) -> str | None:
        """Mean position and horizontal RMS scatter of the collected fixes."""
        if len(self.positions) < 5:
            return None
        import math
        n = len(self.positions)
        mlat = sum(p[0] for p in self.positions) / n
        mlon = sum(p[1] for p in self.positions) / n
        m_lat = 111_320.0
        m_lon = 111_320.0 * math.cos(math.radians(mlat))
        d2 = [((p[0] - mlat) * m_lat) ** 2 + ((p[1] - mlon) * m_lon) ** 2
              for p in self.positions]
        rms = math.sqrt(sum(d2) / n)
        dmax = math.sqrt(max(d2))
        return (f"prumer {mlat:+.6f},{mlon:+.6f} z {n} fixu; "
                f"rozptyl RMS {rms:.1f} m, max {dmax:.1f} m")

    def summary(self) -> list[str]:
        out = ["--- gps summary ---", f"  {self.status_line()}"]
        out.extend(self.sat_table())
        sc = self.scatter()
        if sc:
            out.append(f"  {sc}")
        kinds = ", ".join(f"{k}x{n}" for k, n in sorted(self.types.items()))
        out.append(f"  sentence types: {kinds or 'none'}")
        for line in self.pstm:
            out.append(f"  {line}")
        return out


# --- capture loop ------------------------------------------------------------

def run_gps(ser, sec: int, baud: int, *, nmea_file: Path | None,
            quiet_raw: bool = False) -> tuple[GpsState, str | None]:
    """Send `gps <sec> <baud>`, parse until GPSEND. Returns (state, trailer)."""
    state = GpsState()
    sent = f"gps {sec} {baud}"
    ser.reset_input_buffer()
    ser.write(sent.encode("ascii") + b"\n")
    ser.flush()
    fh = nmea_file.open("w", encoding="utf-8", newline="\n") if nmea_file else None
    trailer: str | None = None
    buf = b""
    deadline = time.monotonic() + sec + 20.0
    next_status = time.monotonic() + 1.0
    next_table = time.monotonic() + 12.0
    try:
        while time.monotonic() < deadline:
            chunk = ser.read(4096)
            if chunk:
                buf += chunk
                while b"\n" in buf:
                    raw, buf = buf.split(b"\n", 1)
                    line = clean(raw.decode("ascii", "replace"))
                    if not line or line == sent:
                        continue
                    if line.startswith("GPSEND") or line.startswith("GPSERR"):
                        trailer = line
                        return state, trailer
                    if line.startswith("GPS "):
                        log(f"  {line}")
                        continue
                    if line.startswith("$"):
                        if fh:
                            fh.write(line + "\n")
                        state.update(line)
                        if (not quiet_raw and line[1:5] == "PSTM"
                                and not line.startswith("$PSTMCPU")):
                            log(f"  | {line}")
            if time.monotonic() >= next_status:
                next_status += 1.0
                if not quiet_raw:
                    log(f"  {state.status_line()}")
            if time.monotonic() >= next_table:
                next_table += 12.0
                if not quiet_raw:
                    for row in state.sat_table():
                        log(row)
        log("  ! deadline hit without GPSEND trailer")
        return state, trailer
    finally:
        if fh:
            fh.close()


def cmd_capture(args: argparse.Namespace) -> int:
    GPS_LOG_DIR.mkdir(parents=True, exist_ok=True)
    nmea_file = GPS_LOG_DIR / f"gps_{datetime.now():%Y%m%d_%H%M%S}.nmea"
    ser, _ = connect()
    try:
        log(f"running `gps {args.sec} {args.baud}` (log: {nmea_file})")
        state, trailer = run_gps(ser, args.sec, args.baud, nmea_file=nmea_file)
    finally:
        ser.close()
    for line in state.summary():
        log(line)
    log(f"  {trailer or '! no trailer'}")
    log(f"  raw NMEA: {nmea_file}")
    return 0 if state.cksum_ok else 1


def cmd_scan(args: argparse.Namespace) -> int:
    ser, _ = connect()
    results: list[tuple[int, GpsState, str | None]] = []
    try:
        for baud in SCAN_BAUDS:
            log(f"--- trying {baud} Bd ---")
            state, trailer = run_gps(ser, 4, baud, nmea_file=None, quiet_raw=True)
            log(f"  {state.status_line()}")
            log(f"  {trailer or '! no trailer'}")
            results.append((baud, state, trailer))
            if state.cksum_ok >= 3 and not args.all:
                break
    finally:
        ser.close()
    log("--- scan result ---")
    best = max(results, key=lambda r: r[1].cksum_ok)
    for baud, state, trailer in results:
        mark = " <== NMEA OK" if state is best[1] and state.cksum_ok else ""
        log(f"  {baud:>6} Bd: valid={state.cksum_ok}/{state.sentences} "
            f"({trailer or 'no trailer'}){mark}")
    if best[1].cksum_ok:
        log(f"module talks NMEA at {best[0]} Bd")
        return 0
    log("no valid NMEA at any baud - check wiring/uarterr counters")
    return 1


def _gpstx(ser, sentence: str, baud: int) -> None:
    line = f"gpstx {sentence} {baud}"
    log(f"sending `{line}`")
    ser.write(line.encode("ascii") + b"\n")
    ser.flush()
    time.sleep(0.3)  # reply lands in the fw RX ring, drained by the next `gps`


# $PSTMGETSWVER wants a component id (7 = whole-system report); send a couple
# of variants since the ROM's accepted set differs between Teseo binaries.
VER_SENTENCES = ("$PSTMGETSWVER,7", "$PSTMGETSWVER,0", "$PSTMGETSWVER")


def cmd_ver(args: argparse.Namespace) -> int:
    ser, _ = connect()
    try:
        ser.reset_input_buffer()
        for sentence in VER_SENTENCES:
            _gpstx(ser, sentence, args.baud)
        state, trailer = run_gps(ser, 3, args.baud, nmea_file=None)
    finally:
        ser.close()
    for line in state.summary():
        log(line)
    if not state.pstm:
        log("no $PSTM reply - wrong baud or module silent (try `scan`)")
        return 1
    return 0


def cmd_send(args: argparse.Namespace) -> int:
    """Send an arbitrary sentence, then capture briefly to show the reply."""
    ser, _ = connect()
    try:
        ser.reset_input_buffer()
        _gpstx(ser, args.sentence, args.baud)
        state, trailer = run_gps(ser, args.sec, args.baud, nmea_file=None)
    finally:
        ser.close()
    for line in state.summary():
        log(line)
    return 0


def cmd_parse(args: argparse.Namespace) -> int:
    """Re-run the parser over a saved .nmea log and print the summary."""
    state = GpsState()
    for raw in Path(args.file).read_text(encoding="utf-8").splitlines():
        state.update(raw.strip())
    for line in state.summary():
        log(line)
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    sub = ap.add_subparsers(dest="sub", required=True)

    p = sub.add_parser("capture")
    p.add_argument("sec", type=int)
    p.add_argument("--baud", type=int, default=9600)
    p.set_defaults(fn=cmd_capture)

    p = sub.add_parser("scan")
    p.add_argument("--all", action="store_true",
                   help="try every baud even after one already works")
    p.set_defaults(fn=cmd_scan)

    p = sub.add_parser("ver")
    p.add_argument("--baud", type=int, default=9600)
    p.set_defaults(fn=cmd_ver)

    p = sub.add_parser("send")
    p.add_argument("sentence", help="e.g. $PSTMGETSWVER,7 (no spaces)")
    p.add_argument("--sec", type=int, default=3)
    p.add_argument("--baud", type=int, default=9600)
    p.set_defaults(fn=cmd_send)

    p = sub.add_parser("parse")
    p.add_argument("file", help="saved .nmea log to summarize")
    p.set_defaults(fn=cmd_parse)

    args = ap.parse_args()
    try:
        return args.fn(args)
    except (TimeoutError, RuntimeError) as exc:
        log(f"ERROR: {exc}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())

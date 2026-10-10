"""DeviceClient.run_detect: command framing, line streaming and the DETEND trailer."""

from __future__ import annotations

import pytest

from conftest import FakeTransport
from stm32node_cli.protocol.client import DeviceClient
from stm32node_cli.protocol.codec import ProtocolError, StreamAborted
from stm32node_cli.protocol.spec import DETECT_DEFAULT_MODEL_THR_MILLI

# A whole run: a level line, one DRONE window, its alarm transition, then DETEND.
_RUN = (
    b"LVL t=1.000 rms=+0.012\r\n"
    b"DET t=2.567 span=14 dec=+5.234 DRONE\r\n"
    b"ALM t=2.567 ON hits=2/4\r\n"
    b"DETEND windows=1 drones=1 alarms=1 first_drone=2.567 first_alarm=2.567 overrun=0 err=0\r\n"
)


def test_run_detect_sends_command_and_returns_trailer():
    t = FakeTransport(to_read=_RUN)
    lines: list[str] = []
    trailer = DeviceClient(t).run_detect(1, on_line=lines.append)

    assert t.written == b"detect 1\n"
    assert trailer is not None
    assert (trailer.windows, trailer.drones, trailer.alarms) == (1, 1, 1)
    assert trailer.overrun is False and trailer.err is False
    assert trailer.first_drone_s == pytest.approx(2.567)
    assert trailer.first_alarm_s == pytest.approx(2.567)
    # The report lines reach on_line; the terminal DETEND does not.
    assert lines == [
        "LVL t=1.000 rms=+0.012",
        "DET t=2.567 span=14 dec=+5.234 DRONE",
        "ALM t=2.567 ON hits=2/4",
    ]


def test_run_detect_fills_positional_gap_before_dbg():
    # dbg is the 4th positional, so squelch and thr must be present; a gap is
    # filled with the firmware default rather than shifting dbg into thr's slot.
    t = FakeTransport(to_read=b"DETEND windows=0 drones=0 alarms=0 overrun=0 err=0\r\n")
    DeviceClient(t).run_detect(5, dbg=True)
    assert t.written == f"detect 5 3 {DETECT_DEFAULT_MODEL_THR_MILLI} 1\n".encode()


def test_run_detect_sends_the_rule_as_the_fifth_argument():
    # A rule alone fills squelch, thr and dbg with the defaults in front of it.
    t = FakeTransport(to_read=b"DETEND windows=0 drones=0 alarms=0 overrun=0 err=0\r\n")
    DeviceClient(t).run_detect(30, rule="mean4")
    assert t.written == f"detect 30 3 {DETECT_DEFAULT_MODEL_THR_MILLI} 0 mean4\n".encode()
    t = FakeTransport(to_read=b"DETEND windows=0 drones=0 alarms=0 overrun=0 err=0\r\n")
    DeviceClient(t).run_detect(30, squelch_milli=2, thr_milli=7660, dbg=True, rule="1of4")
    assert t.written == b"detect 30 2 7660 1 1of4\n"
    with pytest.raises(ValueError):
        DeviceClient(FakeTransport(to_read=b"")).run_detect(5, rule="sometimes")


def test_run_detect_reports_error_trailer():
    reply = b"DETERR mic start failed\r\nDETEND windows=0 drones=0 alarms=0 overrun=0 err=1\r\n"
    t = FakeTransport(to_read=reply)
    lines: list[str] = []
    trailer = DeviceClient(t).run_detect(3, on_line=lines.append)

    assert trailer is not None and trailer.err is True
    assert lines == ["DETERR mic start failed"]


def test_run_detect_retries_then_fails_on_silence():
    # Total silence = the command never landed -> resend each attempt, then give up.
    t = FakeTransport(to_read=b"")
    with pytest.raises(ProtocolError):
        DeviceClient(t).run_detect(1, retries=3, ack_timeout=0.05)
    assert t.written == b"detect 1\n" * 3


def test_run_detect_no_retry_once_lines_arrive():
    # Once any byte arrives the run has started; the command must not be resent
    # (a duplicate detect would queue behind the running one).
    t = FakeTransport(to_read=_RUN)
    DeviceClient(t).run_detect(1, retries=3, ack_timeout=0.05)
    assert t.written == b"detect 1\n"


def test_run_detect_aborts_and_sends_stop_byte():
    t = FakeTransport(to_read=_RUN)
    with pytest.raises(StreamAborted):
        DeviceClient(t).run_detect(0, should_abort=lambda: True)
    # A byte is written to stop a sec=0 run on the board (after the command line).
    assert t.written == b"detect 0\n\n"


def test_run_detect_rejects_seconds_over_max():
    with pytest.raises(ValueError):
        DeviceClient(FakeTransport()).run_detect(86401)

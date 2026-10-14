"""DeviceClient.run_detect: command framing, line streaming and the DETEND trailer."""

from __future__ import annotations

import pytest

from conftest import FakeTransport
from stm32node_cli.protocol.client import DeviceClient
from stm32node_cli.protocol.codec import ProtocolError, StreamAborted

_DETEND0 = b"DETEND windows=0 drones=0 alarms=0 overrun=0 err=0\r\n"
# The `model` listing with a CRNN pair selected: its threshold is a z, not gbt_m1's 3211.
_LISTING = (
    b"model: gbt_m1     feat=1..78 thr=3211\r\n"
    b"model: or_h     * feat=0..984 thr=3831\r\n"
    b"model: crnn_hl    feat=0..2072 thr=8774\r\n"
)


class _BoardTransport(FakeTransport):
    """Answers each command when it is written, as the board does: the `model`
    listing to `model`, a detect run to `detect`. A FakeTransport preloaded with
    both would let the listing read swallow the run (there is no pause between)."""

    def __init__(self, listing: bytes = _LISTING, run: bytes = _DETEND0) -> None:
        super().__init__()
        self._answers = {b"model": listing, b"detect": run}

    def write(self, data: bytes) -> None:
        super().write(data)
        self.feed(self._answers.get(data.split()[0], b""))


# A whole run: a level line, one DRONE window, its alarm transition with the
# unit's position, then DETEND.
_ALARM = (
    "ALARM t=2.567 lat=50.102057 lon=14.426130 sats=7 hdop=1.2 age=0.4 utc=2026-10-14T11:24:43Z"
)
_RUN = (
    b"LVL t=1.000 rms=+0.012\r\n"
    b"DET t=2.567 span=14 dec=+5.234 DRONE\r\n"
    b"ALM t=2.567 ON hits=2/4\r\n" + _ALARM.encode() + b"\r\n"
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
        _ALARM,
    ]


def test_run_detect_fills_positional_gap_before_dbg():
    # dbg is the 4th positional, so squelch and thr must be present: squelch takes
    # the firmware default, thr the ACTIVE model's own, read off `model` first -
    # not gbt_m1's 3211, which is wrong for every other model.
    t = _BoardTransport()
    DeviceClient(t).run_detect(5, dbg=True)
    assert t.written == b"model\ndetect 5 3 3831 1\n"


def test_run_detect_sends_the_rule_as_the_fifth_argument():
    # A rule alone fills squelch, thr and dbg in front of it (thr from the board).
    t = _BoardTransport()
    DeviceClient(t).run_detect(30, rule="mean4")
    assert t.written == b"model\ndetect 30 3 3831 0 mean4\n"
    # Everything given: no question to the board.
    t = FakeTransport(to_read=_DETEND0)
    DeviceClient(t).run_detect(30, squelch_milli=2, thr_milli=7660, dbg=True, rule="1of4")
    assert t.written == b"detect 30 2 7660 1 1of4\n"
    with pytest.raises(ValueError):
        DeviceClient(FakeTransport(to_read=b"")).run_detect(5, rule="sometimes")


def test_run_detect_without_dbg_or_rule_asks_nothing():
    # The firmware applies the model's own threshold itself when nothing follows.
    t = FakeTransport(to_read=_DETEND0)
    DeviceClient(t).run_detect(5, squelch_milli=1)
    assert t.written == b"detect 5 1\n"


def test_run_detect_refuses_when_the_active_threshold_is_unknown():
    # No `*` line (no answer, or a listing it cannot read): refuse rather than
    # send a threshold that belongs to another model.
    t = _BoardTransport(listing=b"model: gbt_m1     feat=1..78 thr=3211\r\n")
    with pytest.raises(ProtocolError):
        DeviceClient(t).run_detect(5, dbg=True)
    assert t.written == b"model\n"


def test_active_model_threshold_reads_the_starred_line():
    assert DeviceClient(_BoardTransport()).active_model_threshold() == 3831
    negative = b"model: weird    * feat=0..9 thr=-1500\r\n"
    assert DeviceClient(_BoardTransport(listing=negative)).active_model_threshold() == -1500


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

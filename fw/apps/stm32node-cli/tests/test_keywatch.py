"""keypress_abort: no-TTY path and terminal restore (the live watcher thread is
not automated - it would need a real tty)."""

from __future__ import annotations

import io
import sys

import pytest

from stm32node_cli.keywatch import keypress_abort


def test_no_tty_yields_callback_that_never_fires(monkeypatch):
    # Under a pipe/redirect stdin is not a terminal, so no watcher is armed and
    # should_abort must stay False (the CLI then relies on Ctrl-C).
    monkeypatch.setattr("sys.stdin", io.StringIO(""))
    with keypress_abort() as should_abort:
        assert should_abort() is False


class _FakeTtyStdin:
    """Just enough of a TTY-looking stdin for the POSIX branch to proceed."""

    def isatty(self) -> bool:
        return True

    def fileno(self) -> int:
        return 0


@pytest.mark.skipif(sys.platform == "win32", reason="POSIX termios branch")
def test_terminal_restored_when_setcbreak_raises(monkeypatch):
    # If switching to cbreak fails partway (odd pty, resource limits), the
    # terminal must still be restored - setcbreak runs inside the try/finally.
    # All termios/tty calls are patched, so no real terminal is touched.
    import termios
    import tty

    sentinel_old = ["saved-attrs"]
    restored: list[tuple[int, object]] = []

    monkeypatch.setattr("sys.stdin", _FakeTtyStdin())
    monkeypatch.setattr(termios, "tcgetattr", lambda fd: sentinel_old)
    monkeypatch.setattr(termios, "tcsetattr", lambda fd, when, attrs: restored.append((fd, attrs)))

    def boom(fd):
        raise termios.error("setcbreak failed")

    monkeypatch.setattr(tty, "setcbreak", boom)

    with pytest.raises(termios.error):
        with keypress_abort():
            pass  # never reached - setcbreak raises on entry

    assert restored == [(0, sentinel_old)]

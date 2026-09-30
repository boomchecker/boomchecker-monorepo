"""Watch the terminal for a single keypress, to stop a running command from the CLI.

The board's ``detect`` (and a ``sec=0`` run in particular) stops as soon as it
receives any byte; :meth:`DeviceClient.run_detect` sends that byte when its
``should_abort`` callback turns True. In the TUI that callback is wired to the
``q`` key; this module gives the plain CLI the same ability without Ctrl-C, so a
field operator can end an open-ended run with a single keypress and still get the
clean ``DETEND`` summary.

:func:`keypress_abort` is a context manager yielding an ``is_pressed()`` callable
suitable to pass straight in as ``should_abort``. It only arms a watcher when
stdin is an interactive terminal; under a pipe/redirect it yields a callback that
never fires (so the caller falls back to Ctrl-C), and it always restores the
terminal mode on the way out.

Single-use per process: the watcher thread is deliberately not joined. When the
run ends without a keypress (the board hit its time limit), the thread stays
parked in its blocking read until the process exits - fine for the one-shot CLI,
but in a long-lived process it would swallow one later keystroke, and a second
``keypress_abort()`` would arm a second reader racing the first for the same fd.
Not re-entrant; use once and let the process end.
"""

from __future__ import annotations

import contextlib
import sys
import threading
from collections.abc import Callable, Iterator


@contextlib.contextmanager
def keypress_abort() -> Iterator[Callable[[], bool]]:
    """Yield a callable that returns True once any key has been pressed.

    A daemon thread blocks on a single character in cbreak/raw mode (POSIX) or via
    ``msvcrt.getch`` (Windows) and sets an event. The key is consumed here, not
    forwarded to the board - the stop byte is the caller's job. No-op (never fires)
    when stdin is not a TTY.
    """
    event = threading.Event()
    if not sys.stdin.isatty():
        yield event.is_set
        return

    try:
        import termios
        import tty
    except ImportError:
        yield from _watch_windows(event)
        return

    fd = sys.stdin.fileno()
    old = termios.tcgetattr(fd)  # non-mutating; capture before entering the try

    def _watch() -> None:
        import os

        with contextlib.suppress(Exception):
            os.read(fd, 1)  # one keypress in cbreak mode
        event.set()

    # setcbreak and the thread start live INSIDE the try: if either raises after
    # the terminal mode has (partially) changed, the finally still restores it.
    try:
        tty.setcbreak(fd)
        threading.Thread(target=_watch, daemon=True).start()
        yield event.is_set
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old)


def _watch_windows(event: threading.Event) -> Iterator[Callable[[], bool]]:
    """Windows keypress watcher (``msvcrt``); no terminal mode to restore."""
    import msvcrt

    def _watch() -> None:
        with contextlib.suppress(Exception):
            msvcrt.getch()
        event.set()

    threading.Thread(target=_watch, daemon=True).start()
    yield event.is_set

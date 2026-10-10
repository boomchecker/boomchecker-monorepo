"""Watch the terminal for a single keypress, to stop a running command from the CLI.

:func:`keypress_abort` yields the ``should_abort`` callback for
:meth:`DeviceClient.run_detect`, which then sends the board one byte and raises
:class:`StreamAborted`. The byte ends a ``detect 0`` run; a timed run ignores it
and finishes on the board. In the TUI, ``q`` then Enter plays the same role.

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

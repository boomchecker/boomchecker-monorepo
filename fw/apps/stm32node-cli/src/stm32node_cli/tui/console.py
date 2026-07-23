"""Console screen: a small REPL for running device commands.

You type commands (``record 5``, ``test 5``, ``version``, ``help``) instead of
clicking through screens. Device commands run in a worker thread; their output
is streamed into the log. Console meta-commands (help/clear/port/quit) are
handled locally.
"""

from __future__ import annotations

from textual import work
from textual.app import ComposeResult
from textual.screen import Screen
from textual.widgets import Footer, Header, Input, ProgressBar, RichLog

from ..sessions.base import CommandContext, get_command, iter_commands


class ConsoleScreen(Screen):
    """Type-a-command console for the connected board."""

    def compose(self) -> ComposeResult:
        yield Header()
        yield RichLog(id="console-log", wrap=True, markup=True)
        yield ProgressBar(id="stream-progress", total=100, show_eta=False)
        yield Input(id="console-input", placeholder="type a command - try 'help'")
        yield Footer()

    def on_mount(self) -> None:
        self.query_one("#stream-progress", ProgressBar).display = False
        self.log_line(f"[b]connected[/b] {self.app.port}")
        self.log_line(f"output folder: {self.app.out_dir}")
        self._print_help()
        self.query_one("#console-input", Input).focus()

    # -- input ---------------------------------------------------------------
    def on_input_submitted(self, event: Input.Submitted) -> None:
        line = event.value.strip()
        self.query_one("#console-input", Input).value = ""
        if not line:
            return
        self.log_line(f"[dim]>[/dim] {line}")
        parts = line.split()
        name, args = parts[0], parts[1:]
        self._dispatch(name, args)

    def _dispatch(self, name: str, args: list[str]) -> None:
        if name in ("quit", "exit"):
            self.app.exit()
        elif name == "clear":
            self.query_one("#console-log", RichLog).clear()
        elif name in ("port", "ports"):
            from .app import ConnectScreen

            self.app.switch_screen(ConnectScreen())
        elif name == "help":
            self._print_help()
        elif get_command(name) is not None:
            self._run_device_command(name, args)
        else:
            self.log_line(f"unknown command: {name} (try 'help')")

    def _print_help(self) -> None:
        self.log_line("[b]commands[/b]")
        for cmd in iter_commands():
            self.log_line(f"  [b]{cmd.usage:<14}[/b] {cmd.help}")
        self.log_line(f"  [b]{'clear':<14}[/b] Clear the log.")
        self.log_line(f"  [b]{'port':<14}[/b] Change the serial port.")
        self.log_line(f"  [b]{'quit':<14}[/b] Exit the app.")

    # -- device command worker ----------------------------------------------
    @work(thread=True, exclusive=True)
    def _run_device_command(self, name: str, args: list[str]) -> None:
        command = get_command(name)
        if command is None:  # pragma: no cover - guarded by caller
            return
        ctx = CommandContext(
            port=self.app.port,
            out_dir=self.app.out_dir,
            emit=self._emit_threadsafe,
            progress=self._progress_threadsafe,
        )
        try:
            command.run(ctx, args)
        except Exception as exc:  # noqa: BLE001 - surface any failure to the log
            self._emit_threadsafe(f"error: {exc}")
        finally:
            self._progress_threadsafe(-1, 0)  # always hide the bar when done

    # -- output --------------------------------------------------------------
    def _emit_threadsafe(self, line: str) -> None:
        """emit() for worker threads: marshal the write onto the UI thread."""
        self.app.call_from_thread(self.log_line, line)

    def _progress_threadsafe(self, done: int, total: int) -> None:
        """progress() for worker threads: marshal the bar update onto the UI."""
        self.app.call_from_thread(self._set_progress, done, total)

    def _set_progress(self, done: int, total: int) -> None:
        bar = self.query_one("#stream-progress", ProgressBar)
        if done < 0 or total <= 0:
            bar.display = False
            return
        bar.display = True
        bar.update(total=total, progress=done)

    def log_line(self, text: str) -> None:
        self.query_one("#console-log", RichLog).write(text)

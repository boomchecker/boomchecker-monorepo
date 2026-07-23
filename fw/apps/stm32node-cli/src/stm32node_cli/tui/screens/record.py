"""Record screen: pick a duration, stream PCM, save a WAV."""

from __future__ import annotations

from textual import work
from textual.app import ComposeResult
from textual.containers import Horizontal, Vertical
from textual.screen import Screen
from textual.widgets import Button, Footer, Header, Input, Label, ProgressBar, Static

from ...config import DEFAULT_TIMEOUT_S
from ...protocol.client import DeviceClient
from ...sessions.base import Feature, FeatureInfo, register_feature
from ...sessions.record import RecordResult, RecordSession
from ...transport.serial_transport import SerialTransport


class RecordScreen(Screen):
    """Stream a fixed number of seconds and write the PCM to a WAV file."""

    BINDINGS = [("escape", "app.pop_screen", "Back")]

    def compose(self) -> ComposeResult:
        yield Header()
        with Vertical(id="record-body"):
            yield Static("Record PCM to WAV", id="record-title")
            with Horizontal(classes="row"):
                yield Label("Seconds:")
                yield Input(value="5", id="seconds", type="integer")
            yield Button("Record", id="record-btn", variant="primary")
            yield ProgressBar(id="record-progress", total=100, show_eta=False)
            yield Static("Idle.", id="record-status")
        yield Footer()

    def on_button_pressed(self, event: Button.Pressed) -> None:
        if event.button.id == "record-btn":
            self._start()

    def _start(self) -> None:
        raw = self.query_one("#seconds", Input).value.strip()
        try:
            seconds = int(raw)
        except ValueError:
            self._status("Invalid duration - enter a whole number of seconds.")
            return
        if seconds <= 0:
            self._status("Duration must be positive.")
            return
        self.query_one("#record-btn", Button).disabled = True
        self._status(f"Recording {seconds}s from {self.app.port} ...")
        self._do_record(seconds, self.app.port, str(self.app.out_dir))

    @work(thread=True, exclusive=True)
    def _do_record(self, seconds: int, port: str, out_dir: str) -> None:
        try:
            with SerialTransport(port, timeout=DEFAULT_TIMEOUT_S) as transport:
                client = DeviceClient(transport)
                session = RecordSession(client, out_dir)
                result = session.record(seconds, on_progress=self._progress_from_thread)
        except Exception as exc:  # noqa: BLE001 - surface any failure to the UI
            self.app.call_from_thread(self._on_error, exc)
            return
        self.app.call_from_thread(self._on_done, result)

    # -- thread -> UI marshalling -------------------------------------------
    def _progress_from_thread(self, done: int, total: int) -> None:
        self.app.call_from_thread(self._update_progress, done, total)

    def _update_progress(self, done: int, total: int) -> None:
        bar = self.query_one("#record-progress", ProgressBar)
        bar.update(total=total or 100, progress=done)

    def _on_done(self, result: RecordResult) -> None:
        self._status(
            f"Saved {result.path.name} "
            f"({result.duration_s:.1f}s, {result.sample_count} samples)."
        )
        self.query_one("#record-btn", Button).disabled = False

    def _on_error(self, exc: Exception) -> None:
        self._status(f"Error: {exc}")
        self.query_one("#record-btn", Button).disabled = False

    def _status(self, message: str) -> None:
        self.query_one("#record-status", Static).update(message)


register_feature(
    Feature(
        info=FeatureInfo(
            key="record",
            title="Record",
            description="Stream N seconds of PCM and save it as a WAV file.",
        ),
        screen_factory=RecordScreen,
    )
)

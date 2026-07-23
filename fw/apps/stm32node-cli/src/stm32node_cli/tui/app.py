"""Textual application: connect screen -> dashboard -> per-feature screens."""

from __future__ import annotations

from pathlib import Path

from textual.app import App, ComposeResult
from textual.containers import Horizontal, Vertical
from textual.screen import Screen
from textual.widgets import Button, Footer, Header, Label, Select, Static

from ..config import DEFAULT_PORT, default_output_dir
from ..sessions.base import REGISTRY, iter_features
from ..transport.serial_transport import list_ports
from . import screens as _screens  # noqa: F401  (imports register the features)

_FEATURE_PREFIX = "feature-"


class ConnectScreen(Screen):
    """Centered "login"-style card: pick a serial port and connect."""

    def compose(self) -> ComposeResult:
        yield Header()
        with Vertical(id="connect-card"):
            yield Static("Connect to boomchecker-node", id="connect-title")
            yield Label("Serial port")
            yield Select([], prompt="Select a port", id="port-select")
            yield Static("", id="connect-msg")
            with Horizontal(id="connect-buttons"):
                yield Button("Refresh", id="refresh-btn")
                yield Button("Connect", id="connect-btn", variant="primary")
        yield Footer()

    def on_mount(self) -> None:
        self._refresh_ports()

    def _refresh_ports(self) -> None:
        select = self.query_one("#port-select", Select)
        msg = self.query_one("#connect-msg", Static)
        connect_btn = self.query_one("#connect-btn", Button)

        options = [
            (f"{p.device}  ({p.description})" if p.description else p.device, p.device)
            for p in list_ports()
        ]
        select.set_options(options)

        if not options:
            msg.update("No serial ports found. Plug in the board and press Refresh.")
            connect_btn.disabled = True
            return

        msg.update("")
        connect_btn.disabled = False
        devices = [device for _label, device in options]
        select.value = self.app.port if self.app.port in devices else options[0][1]

    def on_button_pressed(self, event: Button.Pressed) -> None:
        if event.button.id == "refresh-btn":
            self._refresh_ports()
        elif event.button.id == "connect-btn":
            self._connect()

    def _connect(self) -> None:
        value = self.query_one("#port-select", Select).value
        if value is Select.BLANK or value is None:
            self.query_one("#connect-msg", Static).update("Pick a port first.")
            return
        self.app.port = str(value)
        self.app.switch_screen(DashboardScreen())


class DashboardScreen(Screen):
    """Shows the connected port and the registered features."""

    def compose(self) -> ComposeResult:
        yield Header()
        with Vertical(id="dashboard"):
            yield Static("boomchecker-node - device tools", id="dashboard-title")
            yield Label(f"Connected: {self.app.port}", id="dashboard-port")
            yield Label(f"Output: {self.app.out_dir}", id="dashboard-out")
            yield Static("Features", id="dashboard-features-title")
            for feat in iter_features():
                yield Button(
                    f"{feat.info.title} - {feat.info.description}",
                    id=f"{_FEATURE_PREFIX}{feat.info.key}",
                )
            yield Button("Change port", id="change-port-btn")
        yield Footer()

    def on_button_pressed(self, event: Button.Pressed) -> None:
        button_id = event.button.id or ""
        if button_id == "change-port-btn":
            self.app.switch_screen(ConnectScreen())
            return
        if button_id.startswith(_FEATURE_PREFIX):
            key = button_id[len(_FEATURE_PREFIX) :]
            feature = REGISTRY.get(key)
            if feature is not None:
                self.app.push_screen(feature.screen_factory())


class NodeApp(App):
    """Top-level app holding the shared device context (port, output folder)."""

    TITLE = "stm32node-cli"
    BINDINGS = [("q", "quit", "Quit")]

    CSS = """
    ConnectScreen {
        align: center middle;
    }
    #connect-card {
        width: 64;
        height: auto;
        padding: 1 2;
        border: round $accent;
        background: $panel;
    }
    #connect-title {
        width: 100%;
        content-align: center middle;
        text-style: bold;
        padding-bottom: 1;
    }
    #port-select {
        width: 100%;
    }
    #connect-msg {
        width: 100%;
        color: $warning;
        padding-top: 1;
    }
    #connect-buttons {
        width: 100%;
        height: auto;
        align: center middle;
        padding-top: 1;
    }
    #connect-buttons Button {
        margin: 0 1;
    }
    #dashboard {
        padding: 1 2;
    }
    #dashboard-title {
        text-style: bold;
        padding-bottom: 1;
    }
    #dashboard-features-title {
        text-style: bold;
        padding: 1 0 0 0;
    }
    #dashboard Button {
        margin: 1 0 0 0;
        width: 100%;
    }
    """

    def __init__(self, port: str | None = None, out_dir: str | Path | None = None) -> None:
        super().__init__()
        self.port = port or DEFAULT_PORT
        self.out_dir = Path(out_dir) if out_dir is not None else default_output_dir()

    def on_mount(self) -> None:
        self.push_screen(ConnectScreen())


def run_tui(port: str | None = None, out_dir: str | Path | None = None) -> None:
    """Launch the TUI (blocking)."""
    NodeApp(port=port, out_dir=out_dir).run()

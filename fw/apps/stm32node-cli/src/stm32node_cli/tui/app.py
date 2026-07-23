"""Textual application: a dashboard that launches per-feature screens."""

from __future__ import annotations

from pathlib import Path

from textual.app import App, ComposeResult
from textual.containers import Vertical
from textual.screen import Screen
from textual.widgets import Button, Footer, Header, Input, Label, Static

from ..config import DEFAULT_PORT, default_output_dir
from ..sessions.base import REGISTRY, iter_features
from . import screens as _screens  # noqa: F401  (imports register the features)

_FEATURE_PREFIX = "feature-"


class DashboardScreen(Screen):
    """Lists the registered features and the active port / output folder."""

    def compose(self) -> ComposeResult:
        yield Header()
        with Vertical(id="dashboard"):
            yield Static("boomchecker-node - device tools", id="dashboard-title")
            yield Label(f"Port: {self.app.port}", id="dashboard-port")
            yield Label(f"Output: {self.app.out_dir}", id="dashboard-out")
            yield Input(value=self.app.port, placeholder="serial port", id="port-input")
            for feat in iter_features():
                yield Button(
                    f"{feat.info.title} - {feat.info.description}",
                    id=f"{_FEATURE_PREFIX}{feat.info.key}",
                )
        yield Footer()

    def on_input_changed(self, event: Input.Changed) -> None:
        if event.input.id == "port-input":
            self.app.port = event.value.strip()
            self.query_one("#dashboard-port", Label).update(f"Port: {self.app.port}")

    def on_button_pressed(self, event: Button.Pressed) -> None:
        button_id = event.button.id or ""
        if button_id.startswith(_FEATURE_PREFIX):
            key = button_id[len(_FEATURE_PREFIX) :]
            feature = REGISTRY.get(key)
            if feature is not None:
                self.app.push_screen(feature.screen_factory())


class NodeApp(App):
    """Top-level app holding the shared device context (port, output folder)."""

    TITLE = "stm32node-cli"
    BINDINGS = [("q", "quit", "Quit")]

    def __init__(self, port: str | None = None, out_dir: str | Path | None = None) -> None:
        super().__init__()
        self.port = port or DEFAULT_PORT
        self.out_dir = Path(out_dir) if out_dir is not None else default_output_dir()

    def on_mount(self) -> None:
        self.push_screen(DashboardScreen())


def run_tui(port: str | None = None, out_dir: str | Path | None = None) -> None:
    """Launch the TUI (blocking)."""
    NodeApp(port=port, out_dir=out_dir).run()

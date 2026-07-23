"""TUI navigation: connect screen -> dashboard -> feature screen.

Uses Textual's headless pilot. ``list_ports`` is patched so the flow is
deterministic and does not depend on real hardware being present.
"""

from __future__ import annotations

import asyncio

from textual.widgets import Button

from stm32node_cli.transport.serial_transport import PortInfo
from stm32node_cli.tui import app as app_module
from stm32node_cli.tui.app import NodeApp


def _screen_name(app: NodeApp) -> str:
    return type(app.screen).__name__


def test_connect_then_open_record(monkeypatch):
    monkeypatch.setattr(app_module, "list_ports", lambda: [PortInfo("/dev/ttyFAKE", "fake")])

    async def flow() -> None:
        app = NodeApp()
        async with app.run_test() as pilot:
            await pilot.pause()
            assert _screen_name(app) == "ConnectScreen"

            await pilot.click("#connect-btn")
            await pilot.pause()
            assert _screen_name(app) == "DashboardScreen"
            assert app.port == "/dev/ttyFAKE"

            await pilot.click("#feature-record")
            await pilot.pause()
            assert _screen_name(app) == "RecordScreen"

            await pilot.press("escape")
            await pilot.pause()
            assert _screen_name(app) == "DashboardScreen"

    asyncio.run(flow())


def test_connect_disabled_without_ports(monkeypatch):
    monkeypatch.setattr(app_module, "list_ports", lambda: [])

    async def flow() -> None:
        app = NodeApp()
        async with app.run_test() as pilot:
            await pilot.pause()
            assert app.screen.query_one("#connect-btn", Button).disabled is True
            # Clicking a disabled connect must not navigate away.
            await pilot.click("#connect-btn")
            await pilot.pause()
            assert _screen_name(app) == "ConnectScreen"

    asyncio.run(flow())

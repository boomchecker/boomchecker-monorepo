"""TUI flow: connect screen -> command console.

Uses Textual's headless pilot. ``list_ports`` is patched so the flow is
deterministic and does not depend on real hardware. Only hardware-free paths
(meta-commands, dispatch, help) are exercised; device I/O is covered by the
protocol/session tests.
"""

from __future__ import annotations

import asyncio

from textual.widgets import Button, Input, RichLog

from stm32node_cli.transport.serial_transport import PortInfo
from stm32node_cli.tui import app as app_module
from stm32node_cli.tui.app import NodeApp


def _screen_name(app: NodeApp) -> str:
    return type(app.screen).__name__


def _log_text(app: NodeApp) -> str:
    log = app.screen.query_one("#console-log", RichLog)
    return "\n".join(strip.text for strip in log.lines)


async def _submit(pilot, app: NodeApp, line: str) -> None:
    inp = app.screen.query_one("#console-input", Input)
    app.screen.set_focus(inp)
    inp.value = line
    await pilot.press("enter")
    await pilot.pause()


def _connect_patch(monkeypatch) -> None:
    monkeypatch.setattr(app_module, "list_ports", lambda: [PortInfo("/dev/ttyFAKE", "fake")])


def test_connect_opens_console_with_help(monkeypatch):
    _connect_patch(monkeypatch)

    async def flow() -> None:
        app = NodeApp()
        async with app.run_test() as pilot:
            await pilot.pause()
            assert _screen_name(app) == "ConnectScreen"

            await pilot.click("#connect-btn")
            await pilot.pause()
            assert _screen_name(app) == "ConsoleScreen"
            assert app.port == "/dev/ttyFAKE"

            # Help is printed on mount and lists the registered device commands.
            text = _log_text(app)
            assert "/dev/ttyFAKE" in text
            for name in ("record", "test", "version"):
                assert name in text

    asyncio.run(flow())


def test_unknown_command_reports_error(monkeypatch):
    _connect_patch(monkeypatch)

    async def flow() -> None:
        app = NodeApp()
        async with app.run_test() as pilot:
            await pilot.click("#connect-btn")
            await pilot.pause()
            await _submit(pilot, app, "bogus")
            assert "unknown command: bogus" in _log_text(app)

    asyncio.run(flow())


def test_clear_empties_the_log(monkeypatch):
    _connect_patch(monkeypatch)

    async def flow() -> None:
        app = NodeApp()
        async with app.run_test() as pilot:
            await pilot.click("#connect-btn")
            await pilot.pause()
            assert _log_text(app) != ""
            await _submit(pilot, app, "clear")
            assert _log_text(app) == ""

    asyncio.run(flow())


def test_port_command_returns_to_connect(monkeypatch):
    _connect_patch(monkeypatch)

    async def flow() -> None:
        app = NodeApp()
        async with app.run_test() as pilot:
            await pilot.click("#connect-btn")
            await pilot.pause()
            await _submit(pilot, app, "port")
            assert _screen_name(app) == "ConnectScreen"

    asyncio.run(flow())


def test_connect_disabled_without_ports(monkeypatch):
    monkeypatch.setattr(app_module, "list_ports", lambda: [])

    async def flow() -> None:
        app = NodeApp()
        async with app.run_test() as pilot:
            await pilot.pause()
            assert app.screen.query_one("#connect-btn", Button).disabled is True
            await pilot.click("#connect-btn")
            await pilot.pause()
            assert _screen_name(app) == "ConnectScreen"

    asyncio.run(flow())

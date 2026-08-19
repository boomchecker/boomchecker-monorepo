"""Command-line entry point (Typer). Default action launches the TUI."""

from __future__ import annotations

from pathlib import Path

import typer

from .config import DEFAULT_PORT, DEFAULT_TIMEOUT_S, default_output_dir
from .protocol.spec import STREAM_MAX_SECONDS

app = typer.Typer(
    add_completion=False,
    help="Host tools for the boomchecker-node STM32 board (USB CDC serial).",
)

# Evaluated once at import (as Typer would anyway) so it is a stable default.
_DEFAULT_OUT = default_output_dir()


@app.callback(invoke_without_command=True)
def _default(ctx: typer.Context) -> None:
    """Launch the TUI when invoked without a subcommand."""
    if ctx.invoked_subcommand is None:
        from .tui.app import run_tui

        run_tui(DEFAULT_PORT, _DEFAULT_OUT)


@app.command()
def tui(
    port: str = typer.Option(DEFAULT_PORT, "--port", "-p", help="Serial port."),
    out: Path = typer.Option(_DEFAULT_OUT, "--out", "-o", help="Output folder for recordings."),
) -> None:
    """Launch the interactive TUI."""
    from .tui.app import run_tui

    run_tui(port, out)


@app.command()
def record(
    seconds: int = typer.Argument(
        ..., min=1, max=STREAM_MAX_SECONDS, help="Seconds of audio to record."
    ),
    port: str = typer.Option(DEFAULT_PORT, "--port", "-p", help="Serial port."),
    out: Path = typer.Option(_DEFAULT_OUT, "--out", "-o", help="Output folder for recordings."),
    test_tone: bool = typer.Option(
        False,
        "--test-tone",
        "-t",
        help="Stream a synthetic 1 kHz tone (streamtest) instead of the microphone.",
    ),
) -> None:
    """Record N seconds of PCM to a WAV file (headless)."""
    from .protocol.client import DeviceClient
    from .sessions.record import RecordSession
    from .transport.serial_transport import SerialTransport

    source = "test" if test_tone else "mic"
    with SerialTransport(port, timeout=DEFAULT_TIMEOUT_S) as transport:
        client = DeviceClient(transport)
        session = RecordSession(client, out)
        result = session.record(seconds, source=source)
    typer.echo(f"Saved {result.path} ({result.duration_s:.1f}s, {result.sample_count} samples).")


@app.command()
def ports() -> None:
    """List available serial ports."""
    from .transport.serial_transport import list_ports

    found = list_ports()
    if not found:
        typer.echo("No serial ports found.")
        raise typer.Exit()
    for p in found:
        typer.echo(f"{p.device}\t{p.description}")


@app.command()
def proto(
    out: Path | None = typer.Option(None, "--out", help="Write PROTOCOL.md elsewhere."),
) -> None:
    """Regenerate PROTOCOL.md from the protocol spec."""
    from .protocol import gen_docs

    written = gen_docs.write(out) if out is not None else gen_docs.write()
    typer.echo(f"wrote {written}")


if __name__ == "__main__":
    app()

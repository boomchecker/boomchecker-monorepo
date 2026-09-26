"""Command-line entry point (Typer). Default action launches the TUI."""

from __future__ import annotations

from pathlib import Path

import typer

from .config import DEFAULT_PORT, DEFAULT_TIMEOUT_S, default_output_dir
from .protocol.spec import (
    DETECT_MAX_SECONDS,
    DETECT_SQUELCH_MILLI_MAX,
    DETECT_THR_MILLI_LIMIT,
    STREAM_MAX_SECONDS,
)

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
        ..., min=1, max=STREAM_MAX_SECONDS, help="Seconds of audio per file."
    ),
    count: int = typer.Argument(
        1, min=1, help="How many files of SECONDS each; more than 1 records into one folder."
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
    """Record SECONDS of PCM to a WAV file; `record 10 20` records twenty 10-second files.

    With COUNT > 1 the files land in one folder, each written the moment it
    fills, so Ctrl-C keeps everything recorded so far; index.csv lists them.
    """
    from .protocol.client import DeviceClient
    from .transport.serial_transport import SerialTransport

    source = "test" if test_tone else "mic"
    if count == 1:
        from .sessions.record import RecordSession

        with SerialTransport(port, timeout=DEFAULT_TIMEOUT_S) as transport:
            session = RecordSession(DeviceClient(transport), out)
            result = session.record(seconds, source=source)
        typer.echo(
            f"Saved {result.path} ({result.duration_s:.1f}s, {result.sample_count} samples)."
        )
        return

    from .protocol.codec import StreamAborted
    from .sessions.batch import BatchRecordSession, plan_streams

    plan = plan_streams(seconds, count)
    typer.echo(f"{count} x {seconds}s in {len(plan)} stream(s); Ctrl-C stops and keeps files")

    def on_chunk(index: int, total: int, path: Path) -> None:
        typer.echo(f"saved {path.name} ({index}/{total})")

    try:
        with SerialTransport(port, timeout=DEFAULT_TIMEOUT_S) as transport:
            batch = BatchRecordSession(DeviceClient(transport), out)
            outcome = batch.record(seconds, count, source=source, on_chunk=on_chunk)
    except (StreamAborted, KeyboardInterrupt):
        typer.echo("stopped - finished files are on disk, see index.csv")
        raise typer.Exit(code=1) from None
    typer.echo(
        f"Saved {len(outcome.chunks)}/{count} files in {outcome.directory} "
        f"({outcome.streams} stream(s)); index: {outcome.index_path.name}"
    )


@app.command()
def detect(
    seconds: int = typer.Argument(
        ..., min=0, max=DETECT_MAX_SECONDS, help="Seconds to run; 0 = until Ctrl-C."
    ),
    squelch: int | None = typer.Option(
        None, "--squelch", min=0, max=DETECT_SQUELCH_MILLI_MAX, help="RMS gate, 1/1000."
    ),
    thr: int | None = typer.Option(
        None,
        "--thr",
        min=-DETECT_THR_MILLI_LIMIT,
        max=DETECT_THR_MILLI_LIMIT,
        help="Decision threshold, 1/1000 (default: the selected model's own).",
    ),
    dbg: bool = typer.Option(False, "--dbg", help="Print a per-frame debug line."),
    port: str = typer.Option(DEFAULT_PORT, "--port", "-p", help="Serial port."),
) -> None:
    """Run on-device drone detection, streaming the board's report lines.

    Prints each LVL/DET/ALM line as it arrives and a final DETEND summary. With
    SECONDS 0 the board runs until interrupted; Ctrl-C stops it and prints the
    summary the board reports on the way out.
    """
    from .protocol.client import DeviceClient
    from .protocol.codec import StreamAborted
    from .transport.serial_transport import SerialTransport

    try:
        with SerialTransport(port, timeout=DEFAULT_TIMEOUT_S) as transport:
            trailer = DeviceClient(transport).run_detect(
                seconds,
                squelch_milli=squelch,
                thr_milli=thr,
                dbg=dbg,
                on_line=typer.echo,
            )
    except (KeyboardInterrupt, StreamAborted):
        typer.echo("stopped")
        raise typer.Exit(code=1) from None

    if trailer is None:
        typer.echo("no DETEND trailer - run may be incomplete")
        raise typer.Exit(code=1)
    typer.echo(
        f"{trailer.windows} window(s), {trailer.drones} drone, {trailer.alarms} alarm(s) "
        f"(overrun={int(trailer.overrun)} err={int(trailer.err)})"
    )
    if trailer.err:
        raise typer.Exit(code=1)


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

"""Device commands available in the TUI console.

Each command opens the serial transport, does its work and reports back through
``ctx.emit``. They run in the console's worker thread, so they may block on I/O.
Importing this module registers the commands with the registry in
:mod:`stm32node_cli.sessions.base`.
"""

from __future__ import annotations

from ..config import DEFAULT_TIMEOUT_S
from ..protocol.client import DeviceClient
from ..protocol.codec import StreamAborted
from ..protocol.spec import STREAM_MAX_SECONDS
from ..transport.serial_transport import SerialTransport
from .base import Command, CommandContext, register_command
from .record import RecordSession


def _parse_seconds(ctx: CommandContext, args: list[str], usage: str) -> int | None:
    if len(args) != 1:
        ctx.emit(f"usage: {usage}")
        return None
    try:
        seconds = int(args[0])
    except ValueError:
        ctx.emit(f"usage: {usage} (whole seconds)")
        return None
    if seconds <= 0:
        ctx.emit("duration must be positive")
        return None
    if seconds > STREAM_MAX_SECONDS:
        ctx.emit(f"duration must be <= {STREAM_MAX_SECONDS}s")
        return None
    return seconds


def _record(ctx: CommandContext, args: list[str], *, source: str, usage: str) -> None:
    seconds = _parse_seconds(ctx, args, usage)
    if seconds is None:
        return
    label = "test tone" if source == "test" else "microphone"
    ctx.emit(f"-> requesting {seconds}s of {label} from {ctx.port} ...")

    def on_ack(header) -> None:
        ctx.emit(
            f"[green]v[/green] acknowledged - {header.sample_count} samples "
            f"({header.duration_s:.1f}s) incoming"
        )
        ctx.progress(0, header.byte_length)

    def on_retry(attempt: int, total: int) -> None:
        ctx.emit(
            f"[yellow]no answer (attempt {attempt}/{total}); resending - "
            "press q then Enter to abort[/yellow]"
        )

    try:
        with SerialTransport(ctx.port, timeout=DEFAULT_TIMEOUT_S) as transport:
            session = RecordSession(DeviceClient(transport), ctx.out_dir)
            result = session.record(
                seconds,
                source=source,
                on_ack=on_ack,
                on_progress=ctx.progress,
                should_abort=ctx.should_abort,
                on_retry=on_retry,
            )
    except StreamAborted:
        ctx.emit("[yellow]aborted[/yellow]")
        return

    trailer = result.trailer
    if trailer is None:
        health = " [yellow](no trailer - stream may be incomplete)[/yellow]"
    elif trailer.err:
        health = " [red](WARNING: source produced no data - silence)[/red]"
    elif trailer.overrun:
        health = " [yellow](WARNING: overrun - gaps in capture)[/yellow]"
    else:
        health = " [green](clean)[/green]"
    ctx.emit(
        f"[green]v[/green] saved {result.path.name} "
        f"({result.duration_s:.1f}s, {result.sample_count} samples){health}"
    )


def _cmd_record(ctx: CommandContext, args: list[str]) -> None:
    _record(ctx, args, source="mic", usage="record <sec>")


def _cmd_test(ctx: CommandContext, args: list[str]) -> None:
    _record(ctx, args, source="test", usage="test <sec>")


def _cmd_version(ctx: CommandContext, args: list[str]) -> None:
    if args:
        ctx.emit("usage: version")
        return
    with SerialTransport(ctx.port, timeout=DEFAULT_TIMEOUT_S) as transport:
        version = DeviceClient(transport).version()
    ctx.emit(version or "(no response)")


register_command(
    Command(
        name="record",
        usage="record <sec>",
        help="Record <sec> seconds of microphone PCM and save a WAV.",
        run=_cmd_record,
    )
)
register_command(
    Command(
        name="test",
        usage="test <sec>",
        help="Record <sec> seconds of the synthetic 1 kHz test tone and save a WAV.",
        run=_cmd_test,
    )
)
register_command(
    Command(
        name="version",
        usage="version",
        help="Query the firmware version string.",
        run=_cmd_version,
    )
)

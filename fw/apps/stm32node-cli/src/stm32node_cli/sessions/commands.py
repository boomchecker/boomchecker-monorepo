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
from .batch import BatchRecordSession, plan_streams
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


RECORD_USAGE = "record <sec> [count]"


def _cmd_record(ctx: CommandContext, args: list[str]) -> None:
    """``record 10`` is one WAV; ``record 10 20`` is twenty of them in a folder."""
    if len(args) <= 1:
        _record(ctx, args, source="mic", usage=RECORD_USAGE)
        return
    parsed = _parse_batch(ctx, args, RECORD_USAGE)
    if parsed is None:
        return
    seconds, count = parsed
    if count == 1:
        _record(ctx, args[:1], source="mic", usage=RECORD_USAGE)
        return
    _record_batch(ctx, seconds, count)


def _parse_batch(ctx: CommandContext, args: list[str], usage: str) -> tuple[int, int] | None:
    if len(args) != 2:
        ctx.emit(f"usage: {usage}")
        return None
    try:
        seconds, count = int(args[0]), int(args[1])
    except ValueError:
        ctx.emit(f"usage: {usage} (whole numbers)")
        return None
    if seconds <= 0 or count <= 0:
        ctx.emit("length and count must be positive")
        return None
    if seconds > STREAM_MAX_SECONDS:
        ctx.emit(f"chunk length must be <= {STREAM_MAX_SECONDS}s")
        return None
    return seconds, count


def _record_batch(ctx: CommandContext, seconds: int, count: int) -> None:
    plan = plan_streams(seconds, count)
    ctx.emit(
        f"-> {count} x {seconds}s from {ctx.port} in {len(plan)} stream(s); "
        "files land as they fill - press q then Enter to stop"
    )

    def on_ack(stream_no: int, header) -> None:
        ctx.emit(
            f"[green]v[/green] stream {stream_no}/{len(plan)} acknowledged - "
            f"{header.duration_s:.0f}s incoming"
        )

    def on_chunk(index: int, total: int, path) -> None:
        ctx.emit(f"  saved {path.parent.name}/{path.name} ({index}/{total})")

    def on_retry(attempt: int, total: int) -> None:
        ctx.emit(f"[yellow]no answer (attempt {attempt}/{total}); resending[/yellow]")

    session: BatchRecordSession | None = None
    result = None
    try:
        with SerialTransport(ctx.port, timeout=DEFAULT_TIMEOUT_S) as transport:
            session = BatchRecordSession(DeviceClient(transport), ctx.out_dir)
            result = session.record(
                seconds,
                count,
                on_ack=on_ack,
                on_chunk=on_chunk,
                on_progress=ctx.progress,
                should_abort=ctx.should_abort,
                on_retry=on_retry,
            )
    except StreamAborted:
        ctx.emit("[yellow]aborted - finished chunks are on disk, see index.csv[/yellow]")
        return

    flagged = [
        c for c in result.chunks if c.trailer is not None and (c.trailer.overrun or c.trailer.err)
    ]
    missing = [c for c in result.chunks if c.trailer is None]
    health = " [green](clean)[/green]"
    if flagged:
        health = (
            f" [yellow](WARNING: {len(flagged)} chunk(s) from a stream with overrun/err)[/yellow]"
        )
    elif missing:
        health = " [yellow](no trailer on some stream - check index.csv)[/yellow]"
    ctx.emit(
        f"[green]v[/green] {len(result.chunks)}/{count} files in {result.directory}"
        f" ({result.streams} stream(s)){health}"
    )


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
        usage=RECORD_USAGE,
        help=(
            "Record <sec> seconds of microphone PCM to a WAV; with <count>, record that "
            "many <sec>-second files into one folder, saving each as it fills."
        ),
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

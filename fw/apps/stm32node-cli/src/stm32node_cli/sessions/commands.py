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
from ..protocol.spec import (
    DETECT_MAX_SECONDS,
    DETECT_SQUELCH_MILLI_MAX,
    DETECT_THR_MILLI_LIMIT,
    STREAM_MAX_SECONDS,
)
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


DETECT_USAGE = "detect <sec> [squelch_milli] [thr_milli] [dbg]"


def _parse_detect_args(
    ctx: CommandContext, args: list[str]
) -> tuple[int, int | None, int | None, bool] | None:
    """Parse and range-check ``detect`` arguments; None on a usage error.

    The firmware is the authoritative validator (and applies the selected model's
    own threshold default), so optional arguments are only forwarded when given;
    we check ranges up front only to keep an obvious typo from being mistaken for a
    missing acknowledgement.
    """
    if not 1 <= len(args) <= 4:
        ctx.emit(f"usage: {DETECT_USAGE}")
        return None
    try:
        values = [int(a) for a in args]
    except ValueError:
        ctx.emit(f"usage: {DETECT_USAGE} (whole numbers)")
        return None

    seconds = values[0]
    if not 0 <= seconds <= DETECT_MAX_SECONDS:
        ctx.emit(f"sec must be 0..{DETECT_MAX_SECONDS} (0 = until any key)")
        return None
    squelch = values[1] if len(values) > 1 else None
    if squelch is not None and not 0 <= squelch <= DETECT_SQUELCH_MILLI_MAX:
        ctx.emit(f"squelch_milli must be 0..{DETECT_SQUELCH_MILLI_MAX}")
        return None
    thr = values[2] if len(values) > 2 else None
    if thr is not None and not -DETECT_THR_MILLI_LIMIT <= thr <= DETECT_THR_MILLI_LIMIT:
        ctx.emit(f"thr_milli must be -{DETECT_THR_MILLI_LIMIT}..{DETECT_THR_MILLI_LIMIT}")
        return None
    if len(values) > 3 and values[3] not in (0, 1):
        ctx.emit("dbg must be 0 or 1")
        return None
    dbg = len(values) > 3 and values[3] == 1
    return seconds, squelch, thr, dbg


def _cmd_detect(ctx: CommandContext, args: list[str]) -> None:
    """Run on-device drone detection and stream the report lines to the console."""
    parsed = _parse_detect_args(ctx, args)
    if parsed is None:
        return
    seconds, squelch, thr, dbg = parsed
    ran = "until any key" if seconds == 0 else f"{seconds}s"
    ctx.emit(f"-> detecting ({ran}) on {ctx.port} - press q then Enter to stop")

    def on_line(line: str) -> None:
        alarm_on = line.startswith("ALM") and " ON " in line
        if "DRONE" in line or alarm_on:
            ctx.emit(f"[red]{line}[/red]")
        else:
            ctx.emit(line)

    def on_retry(attempt: int, total: int) -> None:
        ctx.emit(
            f"[yellow]no answer (attempt {attempt}/{total}); resending - "
            "press q then Enter to abort[/yellow]"
        )

    try:
        with SerialTransport(ctx.port, timeout=DEFAULT_TIMEOUT_S) as transport:
            trailer = DeviceClient(transport).run_detect(
                seconds,
                squelch_milli=squelch,
                thr_milli=thr,
                dbg=dbg,
                on_line=on_line,
                should_abort=ctx.should_abort,
                on_retry=on_retry,
            )
    except StreamAborted:
        ctx.emit("[yellow]aborted[/yellow]")
        return

    if trailer is None:
        ctx.emit("[yellow](no DETEND trailer - run may be incomplete)[/yellow]")
        return
    if trailer.err:
        health = " [red](WARNING: detector error - mic failed or disconnected)[/red]"
    elif trailer.overrun:
        health = " [yellow](WARNING: overrun - dropped audio)[/yellow]"
    else:
        health = " [green](clean)[/green]"
    colour = "red" if trailer.drones else "green"
    ctx.emit(
        f"[{colour}]v[/{colour}] {trailer.windows} window(s), {trailer.drones} drone, "
        f"{trailer.alarms} alarm(s){health}"
    )


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
register_command(
    Command(
        name="detect",
        usage=DETECT_USAGE,
        help=(
            "Run on-device drone detection for <sec> seconds (0 = until any key); "
            "streams LVL/DET/ALM lines live and prints a DETEND summary."
        ),
        run=_cmd_detect,
    )
)

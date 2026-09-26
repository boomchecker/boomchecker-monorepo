"""High-level device client: text commands and PCM streaming over a Transport."""

from __future__ import annotations

import contextlib
import re
import time
from collections.abc import Callable, Iterator
from dataclasses import dataclass

from ..transport.base import Transport
from .codec import (
    DetectTrailer,
    ProtocolError,
    StreamAborted,
    StreamHeader,
    StreamTrailer,
    encode_command,
    parse_detect_trailer,
    parse_header,
    parse_trailer,
)
from .spec import (
    DETECT_DEFAULT_MODEL_THR_MILLI,
    DETECT_DEFAULT_SQUELCH_MILLI,
    DETECT_MAX_SECONDS,
    DETECT_TRAILER_PREFIX,
    HEADER_SIZE,
    MAGIC,
    STREAM_MAX_SECONDS,
)

# Strips terminal control sequences the board's console echoes (embedded-cli wraps
# each echoed key in cursor save/restore codes, e.g. b"\x1b[s\x1b[u").
_ANSI_RE = re.compile(r"\x1b\[[0-9;?]*[ -/]*[@-~]")

# Called when an attempt gets no acknowledgement and the command is resent:
# (attempt_that_failed, total_attempts).
RetryFn = Callable[[int, int], None]
# Returns True if the user asked to abort; polled during blocking waits.
AbortFn = Callable[[], bool]
# Called with each text line a streaming text command emits (without its terminator).
LineFn = Callable[[str], None]

# Defaults for the start-of-stream handshake.
DEFAULT_STREAM_RETRIES = 3
DEFAULT_ACK_TIMEOUT_S = 2.0


@dataclass
class StreamHandle:
    """A live PCM stream: its parsed header plus an iterator over payload chunks."""

    header: StreamHeader
    chunks: Iterator[bytes]

    def read_all(self) -> bytes:
        """Drain the stream into a single ``bytes`` object."""
        buf = bytearray()
        for chunk in self.chunks:
            buf += chunk
        return bytes(buf)


class DeviceClient:
    """Talks to the board over a :class:`Transport`.

    The transport is a byte pipe; this class owns the protocol semantics
    (command framing, header resync/parse, payload length handling).
    """

    def __init__(self, transport: Transport) -> None:
        self._t = transport

    # -- lifecycle -----------------------------------------------------------
    def open(self) -> None:
        self._t.open()

    def close(self) -> None:
        self._t.close()

    def __enter__(self) -> DeviceClient:
        self.open()
        return self

    def __exit__(self, *_exc: object) -> None:
        self.close()

    # -- commands ------------------------------------------------------------
    def version(self) -> str:
        """Send ``version`` and return the reported firmware version line."""
        self._t.write(encode_command("version"))
        return self._read_response("version")

    def start_stream(
        self,
        seconds: int,
        *,
        source: str = "mic",
        chunk_size: int = 2048,
        retries: int = DEFAULT_STREAM_RETRIES,
        ack_timeout: float = DEFAULT_ACK_TIMEOUT_S,
        should_abort: AbortFn | None = None,
        on_retry: RetryFn | None = None,
    ) -> StreamHandle:
        """Send a stream command and return a handle to the incoming PCM stream.

        ``source="mic"`` streams the microphone (``stream``); ``source="test"``
        streams a synthetic tone (``streamtest``) for hardware-independent
        verification. Both use identical ``PCM1`` framing.

        Startup handshake: the board's acknowledgement is the ``PCM1`` header
        itself (parsing it confirms the command landed). If it does not arrive
        within ``ack_timeout`` we resend the command, up to ``retries`` times, so
        a missed command self-heals instead of hanging forever. ``should_abort``
        is polled throughout the wait and the transfer: when it returns True (the
        user pressed ``q``) we raise :class:`StreamAborted`.
        """
        if seconds <= 0:
            raise ValueError("seconds must be positive")
        if seconds > STREAM_MAX_SECONDS:
            raise ValueError(f"seconds must be <= {STREAM_MAX_SECONDS}")
        if source not in ("mic", "test"):
            raise ValueError("source must be 'mic' or 'test'")
        if retries < 1:
            raise ValueError("retries must be >= 1")
        command = "streamtest" if source == "test" else "stream"
        encoded = encode_command(command, int(seconds))

        acked = False
        for attempt in range(1, retries + 1):
            self._raise_if_aborted(should_abort)
            self._t.write(encoded)
            if self._await_magic(ack_timeout, should_abort):
                acked = True
                break
            if attempt < retries and on_retry is not None:
                on_retry(attempt, retries)
        if not acked:
            raise ProtocolError(
                f"no response from the board after {retries} attempt(s) - "
                "is it connected and running?"
            )

        # _await_magic consumed the 4-byte PCM1 magic; read the rest of the header.
        rest = self._t.read_exact(HEADER_SIZE - len(MAGIC))
        header = parse_header(MAGIC + rest)
        return StreamHandle(
            header, self._iter_payload(header.byte_length, chunk_size, should_abort)
        )

    def read_trailer(self) -> StreamTrailer | None:
        """Read the ``PCMEND`` trailer sent after the payload.

        Returns the parsed trailer, or None if the board sent none (e.g. the
        stream was aborted) before the transport timed out.
        """
        return parse_trailer(self._read_line())

    def run_detect(
        self,
        seconds: int,
        *,
        squelch_milli: int | None = None,
        thr_milli: int | None = None,
        dbg: bool = False,
        on_line: LineFn | None = None,
        should_abort: AbortFn | None = None,
        on_retry: RetryFn | None = None,
        retries: int = DEFAULT_STREAM_RETRIES,
        ack_timeout: float = DEFAULT_ACK_TIMEOUT_S,
    ) -> DetectTrailer | None:
        """Run the board's ``detect`` command and stream its report lines.

        Sends ``detect <sec> [squelch_milli] [thr_milli] [dbg]`` and reads the
        ``LVL``/``DET``/``ALM`` (and, with ``dbg``, ``F=``) lines the board emits,
        handing each to ``on_line`` as it arrives. Returns the parsed ``DETEND``
        trailer that always closes the run (even after a ``DETERR`` start failure),
        or None if the board sent no trailer before the transport gave up.

        ``detect`` takes positional arguments, so to pass a later one every earlier
        one must be present; a gap is filled with the firmware default. This only
        matters when ``dbg`` is set without an explicit ``thr_milli``, in which case
        the default model's threshold is sent - right after boot, wrong once
        ``model`` has selected another one.

        Startup handshake mirrors :meth:`start_stream`: the command is resent only
        while the board stays *silent* (it was lost). Once any byte arrives the run
        has started, so a resend would queue a duplicate detect - we stop retrying
        and read on. ``should_abort`` is polled throughout; when it returns True we
        send one byte (which stops a ``sec=0`` run on the board) and raise
        :class:`StreamAborted`.
        """
        if seconds < 0 or seconds > DETECT_MAX_SECONDS:
            raise ValueError(f"seconds must be 0..{DETECT_MAX_SECONDS}")
        if retries < 1:
            raise ValueError("retries must be >= 1")

        args: list[int] = [int(seconds)]
        if squelch_milli is not None or thr_milli is not None or dbg:
            args.append(
                DETECT_DEFAULT_SQUELCH_MILLI if squelch_milli is None else int(squelch_milli)
            )
        if thr_milli is not None or dbg:
            args.append(DETECT_DEFAULT_MODEL_THR_MILLI if thr_milli is None else int(thr_milli))
        if dbg:
            args.append(1)
        encoded = encode_command("detect", *args)

        prefix = DETECT_TRAILER_PREFIX.decode("ascii")
        partial = bytearray()
        attempt = 1
        seen_any = False
        self._t.write(encoded)
        deadline = time.monotonic() + ack_timeout
        while True:
            if should_abort is not None and should_abort():
                with contextlib.suppress(Exception):
                    self._t.write(b"\n")  # stop a `sec=0` run on the board
                raise StreamAborted("aborted by user")
            b = self._t.read(1)
            if not b:
                # Silent read. Resend only while nothing has arrived at all (the
                # command was lost); once the run is under way, LVL lines pace it
                # ~once a second, so keep waiting for the DETEND trailer.
                if not seen_any and time.monotonic() >= deadline:
                    if attempt >= retries:
                        raise ProtocolError(
                            f"no response from the board after {retries} attempt(s) - "
                            "is it connected and running?"
                        )
                    attempt += 1
                    if on_retry is not None:
                        on_retry(attempt - 1, retries)
                    self._t.write(encoded)
                    deadline = time.monotonic() + ack_timeout
                continue
            seen_any = True
            if b == b"\r":
                continue
            if b != b"\n":
                partial += b
                continue
            line = partial.decode("ascii", errors="replace")
            partial.clear()
            if line.startswith(prefix):
                return parse_detect_trailer(line)
            if line and on_line is not None:
                on_line(line)

    def _read_response(self, sent: str, *, max_lines: int = 8) -> str:
        """Read a text command's reply, skipping the board's echo and prompt.

        embedded-cli echoes every received character (wrapped in cursor
        save/restore escapes) and prints a ``> `` prompt, so the first line(s)
        after a command are the echo, not the answer. With live autocompletion
        enabled the board also echoes, after each typed character, the
        autocomplete suffix of the command (e.g. typing ``version`` emits
        ``version`` then ``ersion``, ``rsion``, ``sion`` ...). Once the escapes
        and ``\\r`` are stripped these collapse onto one line that *starts with*
        the sent command but is not equal to it (``versionersionrsion...``).

        Return the first line that, once ANSI escapes and a leading prompt are
        stripped, is non-empty and does not start with the echoed command - so
        both a clean echo and an autocompletion-mangled one are skipped, while a
        genuine reply (which never begins with the command word) is returned.
        """
        target = sent.replace(" ", "")
        for _ in range(max_lines):
            line = _ANSI_RE.sub("", self._read_line())
            if line.startswith("> "):
                line = line[2:]
            line = line.strip()
            if line and not line.replace(" ", "").startswith(target):
                return line
        return ""

    # -- internals -----------------------------------------------------------
    def _read_line(self) -> str:
        """Read bytes until a newline; returns the decoded line without it."""
        buf = bytearray()
        while True:
            b = self._t.read(1)
            if not b:
                # Transport timed out / closed; return what we have so far.
                break
            if b == b"\n":
                break
            if b != b"\r":
                buf += b
        return buf.decode("ascii", errors="replace")

    @staticmethod
    def _raise_if_aborted(should_abort: AbortFn | None) -> None:
        if should_abort is not None and should_abort():
            raise StreamAborted("aborted by user")

    def _await_magic(self, ack_timeout: float, should_abort: AbortFn | None) -> bool:
        """Wait for the board's acknowledgement: the ``PCM1`` header magic.

        Discards any echoed command / prompt text, then returns True once the
        4-byte magic has been consumed (the caller reads the rest of the header).

        Returns False (so the caller resends) only if the board stays *silent* -
        nothing arrives within ``ack_timeout``. Once any byte has arrived the
        command has landed, so a slow/warming-up header must NOT trigger a resend:
        that would queue a duplicate ``stream`` on the board and play an extra
        capture. In that case we keep waiting for the magic until the window
        elapses instead of retrying.
        """
        deadline = time.monotonic() + ack_timeout
        window = bytearray()
        seen = False
        while True:
            self._raise_if_aborted(should_abort)
            b = self._t.read(1)
            if not b:
                # Silent read. Retry only if nothing has arrived at all (command
                # lost); otherwise the command landed - wait out the window.
                if not seen or time.monotonic() >= deadline:
                    return False
                continue
            seen = True
            window += b
            if len(window) > len(MAGIC):
                del window[0]
            if bytes(window) == MAGIC:
                return True
            if time.monotonic() >= deadline:
                return False

    def _iter_payload(
        self, byte_length: int, chunk_size: int, should_abort: AbortFn | None = None
    ) -> Iterator[bytes]:
        remaining = byte_length
        while remaining > 0:
            self._raise_if_aborted(should_abort)
            n = min(chunk_size, remaining)
            chunk = self._t.read_exact(n)
            remaining -= len(chunk)
            yield chunk

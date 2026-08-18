"""Shared helpers for the pytest suite. Not a test module itself (no
`test_`/`_test` in the name, so pytest does not collect it)."""

import re
import subprocess

# header.proto: "protocol_version: BoomProtocol compatibility version,
# initially 1" - matches BOOMLINK_PROTOCOL_VERSION in boomlink_codec.h. Used
# by tests that build a FRESH Envelope and need a valid version; NOT used by
# vectors_spec.py, whose protocol_version values are frozen historical
# literals that must never be rewritten to track this (or anything else).
BOOMLINK_PROTOCOL_VERSION = 1

# Real ASan/UBSan report signatures, matched case-sensitively and anchored
# enough that codec_tool.c's own error messages - which echo user-controlled
# strings like a CLI argument or file path back into stderr (e.g. "encode:
# '%s' is not a valid uint32 protocol_version") - cannot accidentally trip
# one. A bare case-insensitive substring check on "sanitizer" or "runtime
# error:" (this function's previous approach) does not have that property:
# a pytest tmp_path derived from a test function name containing the word
# "sanitizer", or a malformed CLI argument that happens to contain the text
# "runtime error:", would make a perfectly clean, intentional rejection fail
# this check. UBSan's actual runtime error format is a source location
# followed by "runtime error:" (e.g. "boomlink_codec.c:42:7: runtime error:
# ..."), which is what the second pattern anchors on instead of a bare
# substring.
_SANITIZER_STDERR_PATTERNS = (
    re.compile(r"AddressSanitizer"),
    re.compile(r"UndefinedBehaviorSanitizer"),
    re.compile(r"LeakSanitizer"),
    re.compile(r"^\S+:\d+:\d+: runtime error:", re.MULTILINE),
)


def run_codec_tool(codec_tool_path, *args):
    return subprocess.run([codec_tool_path, *args], capture_output=True, text=True, timeout=10)


def assert_clean_rejection(result):
    """A test asserting the tool rejects some input must not accept a
    sanitizer abort as equivalent to a clean, intentional rejection - both
    produce a nonzero exit code, but only a clean rejection proves the code
    fails closed on purpose rather than crashing on a memory-safety/
    undefined-behavior bug on exactly the hostile-input path the sanitizers
    (BOOMLINK_SANITIZE, on by default) exist to catch.

    The stderr signature check below is the LOAD-BEARING one, not a
    secondary layer: on Linux, ASan/UBSan's `abort_on_error` defaults to 0,
    so under this project's `-fno-sanitize-recover=all` build flags a
    sanitizer-caught bug makes the process call `_exit(1)` - an ordinary
    POSITIVE exit code - rather than raising SIGABRT. The returncode-sign
    check is kept as a real, but strictly secondary, safety net: it still
    catches a signal-based crash independent of any sanitizer (e.g. a raw
    SIGSEGV in a build with BOOMLINK_SANITIZE off), or a sanitizer
    configuration on a platform/environment where abort_on_error=1.
    """
    assert result.returncode != 0, f"expected a nonzero exit:\n{result.stderr}"
    if result.returncode < 0:
        raise AssertionError(
            f"process was killed by a signal (returncode={result.returncode}), not a clean "
            f"intentional rejection:\n{result.stderr}"
        )
    for pattern in _SANITIZER_STDERR_PATTERNS:
        assert not pattern.search(result.stderr), (
            "rejection looks like a sanitizer abort, not a clean intentional one "
            f"(matched {pattern.pattern!r} in stderr):\n{result.stderr}"
        )


def parse_kv(stdout):
    fields = {}
    for line in stdout.strip().splitlines():
        key, _, value = line.partition("=")
        fields[key] = value
    return fields


def query_codec_tool_limits(codec_tool_path):
    """The tool's real compiled Ping/Pong payload bounds (its `limits`
    subcommand) - the one place tests should read this from, instead of
    each hardcoding its own copy of nanopb/system.options' max_size."""
    result = run_codec_tool(codec_tool_path, "limits")
    if result.returncode != 0:
        raise RuntimeError(f"codec_tool limits failed: {result.stderr}")
    return {key: int(value) for key, value in parse_kv(result.stdout).items()}

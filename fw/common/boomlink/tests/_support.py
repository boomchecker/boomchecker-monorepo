"""Shared helpers for the pytest suite. Not a test module itself (no
`test_`/`_test` in the name, so pytest does not collect it)."""

import subprocess

# header.proto: "protocol_version: BoomProtocol compatibility version,
# initially 1" - matches BOOMLINK_PROTOCOL_VERSION in boomlink_codec.h. Used
# by tests that build a FRESH Envelope and need a valid version; NOT used by
# vectors_spec.py, whose protocol_version values are frozen historical
# literals that must never be rewritten to track this (or anything else).
BOOMLINK_PROTOCOL_VERSION = 1


def run_codec_tool(codec_tool_path, *args):
    return subprocess.run([codec_tool_path, *args], capture_output=True, text=True, timeout=10)


def assert_clean_rejection(result):
    """A test asserting the tool rejects some input must not accept a
    sanitizer abort as equivalent to a clean, intentional rejection - both
    produce a nonzero exit code, but only a clean rejection proves the code
    fails closed on purpose rather than crashing on a memory-safety/
    undefined-behavior bug on exactly the hostile-input path the sanitizers
    (BOOMLINK_SANITIZE, on by default) exist to catch.

    Checks the returncode's sign first: a sanitizer abort terminates the
    process via a signal (SIGABRT under `-fno-sanitize-recover=all`), which
    `subprocess` reports as a NEGATIVE returncode - unlike this tool's own
    error paths, which always `return` a small positive int (1 or 2). The
    stderr text check is a second, belt-and-braces layer in case a sanitizer
    configuration ever exits some other way.
    """
    assert result.returncode > 0, (
        f"expected a clean nonzero exit, got returncode={result.returncode} "
        "(negative means the process was killed by a signal - likely a "
        f"sanitizer abort, not an intentional rejection):\n{result.stderr}"
    )
    lowered = result.stderr.lower()
    for marker in ("sanitizer", "runtime error:"):
        assert marker not in lowered, (
            f"rejection looks like a sanitizer abort, not a clean intentional "
            f"one (found {marker!r} in stderr):\n{result.stderr}"
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

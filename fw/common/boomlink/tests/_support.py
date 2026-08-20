"""Shared helpers for the pytest suite. Not a test module itself (no
`test_`/`_test` in the name, so pytest does not collect it)."""

import os
import re
import subprocess

# header.proto: "protocol_version: BoomProtocol compatibility version,
# initially 1" - matches BOOMLINK_PROTOCOL_VERSION in boomlink_codec.h. Used
# by tests that build a FRESH Envelope and need a valid version; NOT used by
# vectors_spec.py, whose protocol_version values are frozen historical
# literals that must never be rewritten to track this (or anything else).
BOOMLINK_PROTOCOL_VERSION = 1

# Force both sanitizers to raise SIGABRT on a finding instead of calling
# _exit(1). This is what makes assert_clean_rejection()'s returncode-sign
# check actually work (see its docstring): on Linux `abort_on_error` defaults
# to 0, so by default a sanitizer-caught bug is indistinguishable BY EXIT
# CODE from the tool's own intentional `return 1`, leaving a stderr text
# match as the only detector - and text matching is exactly what turned out
# to be fragile here. Appended last rather than assigned, so an option the
# developer set themselves is preserved while ours still wins (sanitizer
# option parsing is last-occurrence-wins - verified).
_SANITIZER_ABORT_OPTIONS = {
    "ASAN_OPTIONS": "abort_on_error=1",
    "UBSAN_OPTIONS": "abort_on_error=1",
}

# Secondary net only - the returncode sign check above is the primary one.
#
# GCC's UBSan is the awkward case: its report contains NO "UndefinedBehavior-
# Sanitizer" string at all (verified - no SUMMARY line, unlike clang), so the
# source-location pattern is the ONLY thing that can recognize one from text.
# That pattern deliberately does NOT anchor the path as `\S+`: a source path
# containing a space (a real checkout under e.g. "~/My Drive/") would slip
# straight past a `\S+` anchor, silently turning every UBSan finding in the
# codec into a "clean rejection" and defeating the whole point of this
# helper. Matching the path shape loosely instead means a deliberately
# crafted file path could in principle trip this - accepted knowingly: with
# _SANITIZER_ABORT_OPTIONS in force a real sanitizer finding is already
# caught by its signal, so a false positive here costs a confusing test
# failure while a false negative would cost a silently-missed memory-safety
# bug.
_SANITIZER_STDERR_PATTERNS = (
    re.compile(r"AddressSanitizer"),
    re.compile(r"UndefinedBehaviorSanitizer"),
    re.compile(r"LeakSanitizer"),
    re.compile(r"==\d+==ERROR:"),
    re.compile(r"^.+:\d+:\d+: runtime error:", re.MULTILINE),
)

# The only exit codes codec_tool.c uses to reject an input: 2 for a usage/parse
# error, 1 for a rejected payload or an I/O failure. Anything else positive is
# not a rejection - see assert_clean_rejection().
_REJECTION_EXIT_CODES = frozenset({1, 2})


def run_codec_tool(codec_tool_path, *args):
    env = dict(os.environ)
    for name, option in _SANITIZER_ABORT_OPTIONS.items():
        existing = env.get(name)
        env[name] = f"{existing}:{option}" if existing else option
    # errors="replace", not text=True's default of "strict": corrupted process
    # output is a SYMPTOM of the bug class assert_clean_rejection() exists to
    # catch, so decoding it must not be what fails. Observed for real - a
    # memory-safety bug on the rejection path emitted a stray 0xa4 byte, and the
    # test died with a UnicodeDecodeError traceback inside subprocess.py instead
    # of this suite's own diagnostic naming the actual problem.
    return subprocess.run(
        [codec_tool_path, *args],
        capture_output=True,
        text=True,
        errors="replace",
        timeout=10,
        env=env,
    )


def assert_clean_rejection(result):
    """A test asserting the tool rejects some input must not accept a
    sanitizer abort as equivalent to a clean, intentional rejection - both
    produce a nonzero exit code, but only a clean rejection proves the code
    fails closed on purpose rather than crashing on a memory-safety/
    undefined-behavior bug on exactly the hostile-input path the sanitizers
    (BOOMLINK_SANITIZE, which the Debug preset turns on - the CMake option
    itself defaults to OFF, and this whole check is only as good as that
    being enabled; see conftest.py's pytest_report_header) exist to catch.

    The returncode-sign check below is the load-bearing one, and it is only
    load-bearing because run_codec_tool() forces `abort_on_error=1` on both
    sanitizers (see _SANITIZER_ABORT_OPTIONS): a finding then arrives as
    SIGABRT, which `subprocess` reports as a NEGATIVE returncode, while this
    tool's own error paths always `return` a small positive int (1 or 2).
    Without that option the sanitizers call `_exit(1)` on Linux and the sign
    check would be inert - which is why it is not left to depend on the
    platform's default. The stderr signature check is a secondary net for a
    platform or future sanitizer that reports some other way.
    """
    assert result.returncode != 0, f"expected a nonzero exit:\n{result.stderr}"
    if result.returncode < 0:
        raise AssertionError(
            f"process was killed by a signal (returncode={result.returncode}), not a clean "
            f"intentional rejection. With abort_on_error=1 forced on both sanitizers a "
            f"sanitizer finding arrives as SIGABRT (-6); so does codec_tool's own abort() "
            f"on an internal fault, so read the stderr below to tell them apart - a "
            f"sanitizer report names itself, an internal fault says so explicitly:"
            f"\n{result.stderr}"
        )
    # An exact allow-list, not just "positive": codec_tool's rejection paths
    # return only 1 or 2, so any OTHER positive code is something else wearing a
    # rejection's clothes. Accepting all positives once let an internal fault
    # (an exit(70) from hex_encode, over a real intra-object buffer overflow)
    # pass three over-bound tests green.
    assert result.returncode in _REJECTION_EXIT_CODES, (
        f"exit code {result.returncode} is not one of codec_tool's rejection codes "
        f"{sorted(_REJECTION_EXIT_CODES)} - an unexpected positive code means an internal "
        f"fault or a new failure path, not a clean intentional rejection:\n{result.stderr}"
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
    """Every compiled bound the tool can report (its `limits` subcommand) -
    the one place tests should read these from, instead of each hardcoding
    its own copy of a number that lives in nanopb/system.options or
    boomlink_codec.h. Values that parse as integers are returned as ints, any
    other value as the raw string - so a future non-numeric field (a
    `nanopb_version`, a `build_type`) is simply carried through rather than
    breaking every caller. That is not hypothetical caution: coercing EVERY
    value with int() meant one such field raised ValueError here, during
    test_encode_decode.py's collection-time pytest_generate_tests, which
    aborted the whole session before a single test ran.

    Keys today (all ints):

      ping_payload_max / pong_payload_max
          the per-message bounded `bytes` field caps (nanopb/system.options'
          max_size, read back off the generated struct layout).
      envelope_size
          Nanopb's worst-case encoded Envelope size for this schema
          (boomlink_Envelope_size).
      envelope_budget
          the real on-air ceiling: BOOMLINK_RADIO_MAX_PAYLOAD minus
          BOOMLINK_LINK_FRAME_HEADER_SIZE. Larger than envelope_size - the
          gap is what a forward-compatible frame from a newer peer may use.
      decode_read_cap
          how many bytes `decode` will read from a file before rejecting it
          as too large (BOOMLINK_DECODE_READ_CAP).
      sanitizers
          1 if the binary was built with ASan/UBSan (CMake's
          BOOMLINK_SANITIZE), else 0. See conftest.py's
          pytest_report_header() for why the suite surfaces this.
    """
    result = run_codec_tool(codec_tool_path, "limits")
    if result.returncode != 0:
        raise RuntimeError(f"codec_tool limits failed: {result.stderr}")
    limits = {}
    for key, value in parse_kv(result.stdout).items():
        try:
            limits[key] = int(value)
        except ValueError:
            limits[key] = value
    return limits

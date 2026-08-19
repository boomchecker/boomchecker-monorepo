"""Shared fixtures for the BoomProtocol host test suite.

Generated Python Protobuf classes (common_pb2, header_pb2, system_pb2,
envelope_pb2) are not imported here directly - they only exist in the build
tree (see CMakeLists.txt's `boomlink_python_pb2` target) and are found via
the PYTHONPATH the test runner sets, not via any path manipulation in this
file. Import them directly in test modules, e.g. `import envelope_pb2`.
"""

import os

import pytest
from _support import query_codec_tool_limits


def pytest_configure(config):
    """Fail fast with one actionable message covering BOTH ways this
    environment is normally missing, instead of whichever happens to be
    checked/imported first: a bare `import envelope_pb2` failing with an
    unhelpful ModuleNotFoundError at collection time (if PYTHONPATH isn't
    set), or a misleadingly partial-green run if only BOOMLINK_CODEC_TOOL is
    unset (tests not using that fixture would still pass)."""
    del config
    missing = []
    try:
        import envelope_pb2  # noqa: F401
    except ImportError:
        missing.append(
            "  - PYTHONPATH does not include the generated Python protobuf classes "
            "(envelope_pb2 and friends). These are generated at build time, not "
            "committed - see CMakeLists.txt's boomlink_python_pb2 target."
        )
    try:
        import boomlink_linkframe  # noqa: F401
    except ImportError:
        missing.append(
            "  - PYTHONPATH does not include fw/common/boomlink/linkframe, so the "
            "boomlink_linkframe reference parser cannot be imported."
        )
    # Both tools, checked the same way: set, and pointing at something that
    # exists and is executable. Checking non-empty alone is not enough - a value
    # left over from a `rm -rf build` would otherwise surface as a raw
    # FileNotFoundError from whichever test happened to run first.
    for env_var, description in (
        ("BOOMLINK_CODEC_TOOL", "boomlink_codec_tool"),
        ("BOOMLINK_LINKFRAME_TOOL", "boomlink_linkframe_tool"),
    ):
        path = os.environ.get(env_var)
        if not path:
            missing.append(
                f"  - {env_var} is not set. Point it at the compiled {description} binary."
            )
        elif not (os.path.isfile(path) and os.access(path, os.X_OK)):
            missing.append(
                f"  - {env_var}={path!r} does not point to an existing, executable file."
            )
    if missing:
        pytest.exit(
            "Test environment is not set up:\n"
            + "\n".join(missing)
            + "\n\nRun the suite via `ctest` in the CMake build directory (sets both "
            "automatically), or `task test` in fw/common/boomlink (see Taskfile.yml), "
            "rather than invoking pytest directly.",
            returncode=1,
        )


def pytest_report_header(config):
    """Report whether the binary under test was built with sanitizers, in the
    header of every run.

    Not cosmetic: assert_clean_rejection() (see _support.py) rests entirely on
    ASan/UBSan being present - it is what separates "this hostile input was
    rejected on purpose" from "this hostile input crashed the parser". With
    BOOMLINK_SANITIZE=OFF the negative-path tests still pass, because a
    memory-safety bug on a rejection path produces a nonzero exit just like an
    intentional rejection does; verified a deliberate use-after-free passing
    the whole suite green. The option defaults to OFF and only the Debug preset
    turns it ON, so an IDE's default configure or a hand-rolled
    `cmake -S . -B build` silently gets the weaker suite. Reported rather than
    failed: running uninstrumented on purpose is legitimate (it is much
    faster), it just should never be a surprise.
    """
    del config
    codec_tool = os.environ.get("BOOMLINK_CODEC_TOOL")
    if not codec_tool:
        return None
    try:
        limits = query_codec_tool_limits(codec_tool)
    except Exception:  # noqa: BLE001 - see comment
        # Deliberately broad. pytest_configure already validated the binary, so
        # this is a header-line nicety and must not become a second, noisier
        # failure path - which is exactly what a narrow (RuntimeError, OSError)
        # did: `limits` growing one non-integer key made int() raise ValueError
        # here and abort the whole session with INTERNALERROR during
        # pytest_sessionstart, before collection.
        return None
    if "sanitizers" not in limits:
        # Distinct from 0: a binary built before `limits` reported this at all
        # cannot be asked. Saying "built WITHOUT sanitizers" here would state a
        # confident falsehood about a possibly-instrumented binary, and hand out
        # an instruction that would not change anything - and a check that cries
        # wolf gets ignored, which costs the whole point of reporting it.
        return (
            "BoomLink codec_tool: cannot tell whether it was built with sanitizers - this "
            "binary predates the `sanitizers=` field in `codec_tool limits`, so it is "
            "stale relative to the test suite. Rebuild it."
        )
    sanitizers = limits["sanitizers"]
    # Compared against the two values the field is defined to take rather than
    # tested for truthiness: query_codec_tool_limits() passes a value it cannot
    # parse as an int straight through as a string, and a truthiness test would
    # read a future `sanitizers=off` as ON - reporting the exact opposite of the
    # truth, in the one line whose whole job is to be honest about this.
    if sanitizers == 1:
        return "BoomLink codec_tool: built WITH ASan/UBSan"
    if sanitizers == 0:
        return (
            "BoomLink codec_tool: built WITHOUT sanitizers - negative-path tests cannot "
            "tell a clean rejection from a memory-safety bug (configure with "
            "`cmake --preset Debug`, or -DBOOMLINK_SANITIZE=ON, to restore that)"
        )
    return (
        f"BoomLink codec_tool: reported an unrecognized sanitizers value {sanitizers!r} - "
        "cannot tell whether the negative-path tests have a safety net"
    )


@pytest.fixture(scope="session")
def codec_tool_path():
    """Path to the compiled boomlink_codec_tool binary (see
    tests/codec_tool.c) - the "Nanopb side" of the cross-language interop
    tests. pytest_configure() above already guarantees this is set."""
    return os.environ["BOOMLINK_CODEC_TOOL"]


@pytest.fixture(scope="session")
def linkframe_tool_path():
    """Path to the compiled boomlink_linkframe_tool binary (see
    tests/linkframe_tool.c) - the C side of the link frame cross-check against
    the Python reference parser. pytest_configure() guarantees this is set."""
    return os.environ["BOOMLINK_LINKFRAME_TOOL"]


@pytest.fixture(scope="session")
def linkframe_constants(linkframe_tool_path):
    """The link frame's compile-time constants and every parse result code, as
    reported by `linkframe_tool limits` - so the tests compare against the C
    enumerators rather than assuming the Python mirror still matches them."""
    try:
        return query_codec_tool_limits(linkframe_tool_path)
    except RuntimeError as exc:
        pytest.fail(str(exc))


@pytest.fixture(scope="session")
def codec_tool_limits(codec_tool_path):
    """Every compiled bound the tool reports: the Ping/Pong payload caps plus
    envelope_size, envelope_budget, decode_read_cap and sanitizers - see
    _support.query_codec_tool_limits() for what each one means. Tests read
    these instead of hardcoding their own copy of a number defined in
    nanopb/system.options or boomlink_codec.h."""
    try:
        return query_codec_tool_limits(codec_tool_path)
    except RuntimeError as exc:
        pytest.fail(str(exc))

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
    codec_tool = os.environ.get("BOOMLINK_CODEC_TOOL")
    if not codec_tool:
        missing.append(
            "  - BOOMLINK_CODEC_TOOL is not set. Point it at the compiled "
            "boomlink_codec_tool binary."
        )
    elif not (os.path.isfile(codec_tool) and os.access(codec_tool, os.X_OK)):
        # Set but stale/wrong (e.g. left over from a `rm -rf build` since the
        # last time it was exported) - checking non-empty alone isn't enough,
        # or every test using it fails with a raw FileNotFoundError instead
        # of this actionable message.
        missing.append(
            f"  - BOOMLINK_CODEC_TOOL={codec_tool!r} does not point to an "
            "existing, executable file."
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
    except (RuntimeError, OSError):
        # pytest_configure already validated the binary; don't turn a
        # header-line nicety into a second, noisier failure path.
        return None
    if limits.get("sanitizers"):
        return "BoomLink codec_tool: built WITH ASan/UBSan"
    return (
        "BoomLink codec_tool: built WITHOUT sanitizers - negative-path tests cannot "
        "tell a clean rejection from a memory-safety bug (configure with "
        "`cmake --preset Debug`, or -DBOOMLINK_SANITIZE=ON, to restore that)"
    )


@pytest.fixture(scope="session")
def codec_tool_path():
    """Path to the compiled boomlink_codec_tool binary (see
    tests/codec_tool.c) - the "Nanopb side" of the cross-language interop
    tests. pytest_configure() above already guarantees this is set."""
    return os.environ["BOOMLINK_CODEC_TOOL"]


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

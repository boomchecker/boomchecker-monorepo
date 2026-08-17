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


@pytest.fixture(scope="session")
def codec_tool_path():
    """Path to the compiled boomlink_codec_tool binary (see
    tests/codec_tool.c) - the "Nanopb side" of the cross-language interop
    tests. pytest_configure() above already guarantees this is set."""
    return os.environ["BOOMLINK_CODEC_TOOL"]


@pytest.fixture(scope="session")
def codec_tool_limits(codec_tool_path):
    """The tool's real compiled Ping/Pong payload bounds, so tests read the
    limit from the one place it is actually defined - the compiled struct
    layout - instead of each hardcoding its own copy of
    nanopb/system.options' max_size."""
    try:
        return query_codec_tool_limits(codec_tool_path)
    except RuntimeError as exc:
        pytest.fail(str(exc))

"""Shared fixtures for the BoomProtocol host test suite.

Generated Python Protobuf classes (common_pb2, header_pb2, system_pb2,
envelope_pb2) are not imported here directly - they only exist in the build
tree (see CMakeLists.txt's `boomlink_python_pb2` target) and are found via
the PYTHONPATH the test runner sets, not via any path manipulation in this
file. Import them directly in test modules, e.g. `import envelope_pb2`.
"""

import os

import pytest


@pytest.fixture(scope="session")
def codec_tool_path():
    """Path to the compiled boomlink_codec_tool binary (see
    tests/test_encode_decode.c) - the "Nanopb side" of the cross-language
    interop tests."""
    path = os.environ.get("BOOMLINK_CODEC_TOOL")
    if not path:
        pytest.fail(
            "BOOMLINK_CODEC_TOOL is not set. These tests shell out to the "
            "compiled boomlink_codec_tool binary and its Python protobuf "
            "classes; both are generated/built, not committed. Run the "
            "suite via `ctest` in the CMake build directory, or `task test` "
            "in fw/common/boomlink (see Taskfile.yml), rather than invoking "
            "pytest directly."
        )
    return path

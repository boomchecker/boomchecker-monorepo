"""Shared helpers for the pytest suite. Not a test module itself (no
`test_`/`_test` in the name, so pytest does not collect it)."""

import subprocess


def run_codec_tool(codec_tool_path, *args):
    return subprocess.run([codec_tool_path, *args], capture_output=True, text=True, timeout=10)


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

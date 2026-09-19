#!/usr/bin/env python3
"""Print every header a translation unit actually opens, one absolute path per line.

Used by check_no_nanopb.sh to verify BoomLink's link frame layer has no Nanopb
dependency. A symbol check alone cannot see one: a header-only dependency (a
macro, a typedef, `pb_size_t`) emits nothing for `nm` to find, and a relative
include reaching nanopb needs no CMake change, so neither the build config nor
the archive would betray it.

Works by re-running the compiler in preprocess-only dependency mode (-M) with the
translation unit's REAL flags, taken from compile_commands.json. Deliberately not
by reading depfiles: the Ninja generator consumes and deletes those, keeping
dependencies in its own .ninja_deps database, so there is nothing on disk to
grep - and going through the compiler resolves a `../`-spelled include to the
same absolute path an -I one would produce.
"""

import json
import shlex
import subprocess
import sys
from pathlib import Path


def find_entry(compile_commands: Path, source: Path):
    """The compile_commands.json entry for `source`, matched on resolved path so
    a caller may pass either a relative or an absolute path."""
    entries = json.loads(compile_commands.read_text())
    wanted = source.resolve()
    for entry in entries:
        directory = Path(entry["directory"])
        if (directory / entry["file"]).resolve() == wanted:
            return entry
    raise SystemExit(
        f"{source} has no entry in {compile_commands} - it is not compiled by this "
        f"build, or the build directory is stale"
    )


def dependency_argv(entry) -> list[str]:
    """The entry's command with output/compile flags replaced by -M.

    -M rather than -MM on purpose: system headers are listed too. nanopb lives in
    a vendored submodule reached via -I, which some compilers would class as a
    system path if it were ever passed as -isystem, and excluding those would be
    exactly the wrong blind spot for this check.
    """
    argv = entry["arguments"] if "arguments" in entry else shlex.split(entry["command"])
    out: list[str] = []
    skip_next = False
    for arg in argv[1:]:
        if skip_next:
            skip_next = False
            continue
        # -o and -MF/-MT/-MQ each take a separate path argument that must go with
        # them. The dependency flags matter because they REDIRECT the output this
        # script parses: with an -MF in the command, `-M` writes the dependency
        # list to that file and prints nothing at all, leaving this script with an
        # empty result. Today CMake's compile_commands.json carries no dependency
        # flags (Ninja keeps them out of it), so this is defensive - and
        # check_no_nanopb.sh treats an empty include list as a failure, so the
        # worst case was always a confusing failure rather than a silent pass.
        if arg in ("-o", "-MF", "-MT", "-MQ"):
            skip_next = True
            continue
        # -c would compile; -M* variants would fight with the -M added below; the
        # sanitizer/codegen flags are irrelevant to which headers get opened and
        # only slow this down.
        if arg == "-c" or arg.startswith("-fsanitize") or arg.startswith("-M"):
            continue
        out.append(arg)
    return [argv[0], "-M", *out]


def main() -> int:
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} <compile_commands.json> <source>", file=sys.stderr)
        return 2
    compile_commands = Path(sys.argv[1])
    source = Path(sys.argv[2])

    entry = find_entry(compile_commands, source)
    result = subprocess.run(
        dependency_argv(entry),
        cwd=entry["directory"],
        capture_output=True,
        text=True,
        errors="replace",
    )
    if result.returncode != 0:
        print(
            f"preprocessing {source} failed:\n{result.stderr}",
            file=sys.stderr,
        )
        return 1

    # -M output is a makefile rule: "target: dep dep \<newline> dep ...".
    body = result.stdout.split(":", 1)[1] if ":" in result.stdout else result.stdout
    directory = Path(entry["directory"])
    for token in body.replace("\\\n", " ").replace("\\", " ").split():
        # Resolved, so a ../-spelled include and an -I one compare equal.
        print((directory / token).resolve())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

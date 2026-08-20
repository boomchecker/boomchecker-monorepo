#!/bin/sh
# Verify BoomLink's link frame layer has no Nanopb dependency.
#
# boomlink.md section 9: "BoomLink never decodes the Protobuf payload and has no
# Nanopb dependency" - the link layer must be able to filter, acknowledge and
# deduplicate a packet, and reject foreign traffic from a few leading bytes,
# without a Protobuf decoder. CMake enforces the common case by not linking
# Nanopb and not exposing the codec's include path, so a violation is usually a
# compile error. This exists for the cases that are not:
#
#   * a refactor that re-adds the link to make something build - the compile
#     error disappears and every test stays green;
#   * a RELATIVE include that reaches nanopb's headers without any CMake change
#     at all (`#include "../../../bom-stm32node/third_party/nanopb/pb.h"`
#     compiles clean, even under -Werror) - which no symbol check can see if it
#     only uses macros or typedefs.
#
# Registered as a CTest test, not only as a CI step, so `task test` runs it too:
# a boundary checked exclusively in CI gives a developer no signal until they
# have already pushed.
#
# Usage: check_no_nanopb.sh <python> <nanopb-dir> <compile-commands-json> \
#                            <source-under-test> <artefact>...
set -eu

if [ "$#" -lt 5 ]; then
  echo "usage: $0 <python> <nanopb-dir> <compile-commands-json> <source> <artefact>..." >&2
  exit 2
fi

python=$1
nanopb_dir=$2
compile_commands=$3
source_under_test=$4
shift 4

status=0

# 1. Symbol check on each built artefact.
for artefact in "$@"; do
  if [ ! -f "$artefact" ]; then
    # A negative assertion is vacuously true when there is nothing to inspect,
    # so a missing artefact must fail rather than pass. Without this the check
    # silently disarms on exactly the refactor it guards against - the target
    # renamed, folded into another library, or given a different output dir.
    echo "FAIL: $artefact does not exist - this check cannot verify anything" >&2
    status=1
    continue
  fi
  # nm runs as its OWN command, not inside an `if ... | grep` pipeline. In a
  # pipeline its failure is indistinguishable from grep's benign "no match":
  # `set -e` is suspended inside an `if` condition, and pipefail returns
  # grep's 1, so a truncated, zero-byte or wrong-format file reported success.
  if ! symbols=$(nm "$artefact" 2>&1); then
    echo "FAIL: cannot read symbols from $artefact: $symbols" >&2
    status=1
    continue
  fi
  # An empty-but-valid archive is what a target whose sources were removed or
  # folded elsewhere produces: nm succeeds, prints nothing, and the grep below
  # would find nothing to complain about. That is the vacuous pass again.
  if [ -z "$symbols" ]; then
    echo "FAIL: $artefact contains no symbols at all - nothing was verified" >&2
    status=1
    continue
  fi
  # Any symbol CLASS, not just U/T/t. nm's class letter says where a symbol
  # lives, and restricting the pattern to undefined-plus-text quietly exempted
  # the rest: a weak definition (W/V), a common or absolute symbol (C/A), an
  # indirect reference (i/I), or nanopb data landing in .rodata/.bss (R/B/D)
  # would all have passed. No nanopb symbol is weak today, so this is
  # defensive - but "which classes could nanopb's symbols be in" is not a
  # question this check should depend on getting right.
  if printf '%s\n' "$symbols" | grep -E ' [A-Za-z] pb_'; then
    echo "FAIL: $artefact references Nanopb symbols" >&2
    status=1
  else
    echo "ok: $artefact is free of Nanopb symbols"
  fi
done

# 2. Include check, from the compiler itself. This is the half a symbol check
#    cannot do: a header-only dependency (a macro, a typedef, pb_size_t) emits no
#    symbol, so nm sees a clean archive while the layer has quietly grown a
#    Nanopb build dependency - and a RELATIVE include needs no CMake change at
#    all, so nothing else would notice either.
#
#    Re-runs the preprocessor with -M using the translation unit's REAL flags
#    from compile_commands.json, rather than reading depfiles: the Ninja
#    generator consumes and deletes those, keeping dependencies in its own
#    .ninja_deps database, so there is nothing on disk to grep. Going through the
#    compiler also means a relative include spelled with ../ is resolved and
#    caught exactly like an -I one.
if [ ! -f "$compile_commands" ]; then
  echo "FAIL: $compile_commands not found - cannot check includes." >&2
  echo "      Configure with CMAKE_EXPORT_COMPILE_COMMANDS=ON (the Debug preset does)." >&2
  exit 1
fi

if ! includes=$("$python" "$(dirname "$0")/list_includes.py" \
                  "$compile_commands" "$source_under_test"); then
  echo "FAIL: could not determine the includes of $source_under_test" >&2
  exit 1
fi
if [ -z "$includes" ]; then
  echo "FAIL: no includes reported for $source_under_test - nothing was verified" >&2
  exit 1
fi

nanopb_real=$(cd "$nanopb_dir" && pwd -P)
matches=$(printf '%s\n' "$includes" | grep -F "$nanopb_real/" || true)
if [ -n "$matches" ]; then
  echo "FAIL: the link frame layer includes Nanopb headers:" >&2
  printf '  %s\n' "$matches" >&2
  status=1
else
  echo "ok: the link frame layer includes no Nanopb headers"
fi

exit $status

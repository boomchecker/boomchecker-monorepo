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
#            --sources <source>... --artefacts <artefact>...
#
# Both lists are plural, and the sources one especially. It used to take exactly
# one source (`shift 4`), which was fine while one target had to hold this
# property - but the link ENGINE holds it too, and it is four translation units.
# With a single-source interface, adding the engine's archive to the artefact
# list would have covered its symbols while leaving its includes unchecked, which
# is the half of this check that catches a header-only dependency. Verified: with
# the engine's sources absent, a relative include of nanopb's pb.h from
# boomlink_link.c passed the whole suite.
set -eu

if [ "$#" -lt 3 ]; then
  echo "usage: $0 <python> <nanopb-dir> <compile-commands-json>" \
       "--sources <source>... --artefacts <artefact>..." >&2
  exit 2
fi

python=$1
nanopb_dir=$2
compile_commands=$3
shift 3

# Parsed into two space-separated lists rather than arrays: this is /bin/sh, and
# the paths CMake passes are build-tree paths with no spaces in them. Stated
# because it IS a limitation - a build directory with a space in its name would
# break this - and the alternative (re-exec under a shell with arrays) is a
# bigger dependency than the problem.
sources=""
artefacts=""
bucket=""
for arg in "$@"; do
  case "$arg" in
    --sources)   bucket=sources ;;
    --artefacts) bucket=artefacts ;;
    *)
      case "$bucket" in
        sources)   sources="$sources $arg" ;;
        artefacts) artefacts="$artefacts $arg" ;;
        *)
          echo "FAIL: '$arg' came before --sources or --artefacts" >&2
          exit 2
          ;;
      esac
      ;;
  esac
done

# An empty list is the vacuous pass this whole script exists to avoid: a
# refactor that renames a target or moves a source would otherwise leave the
# check running happily over nothing.
if [ -z "$sources" ]; then
  echo "FAIL: no sources given - there would be nothing to check includes for" >&2
  exit 2
fi
if [ -z "$artefacts" ]; then
  echo "FAIL: no artefacts given - there would be nothing to check symbols in" >&2
  exit 2
fi

status=0

# 1. Symbol check on each built artefact.
# shellcheck disable=SC2086 # deliberate word splitting; see the note above
for artefact in $artefacts; do
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

nanopb_real=$(cd "$nanopb_dir" && pwd -P)

# shellcheck disable=SC2086 # deliberate word splitting; see the note above
for source_under_test in $sources; do
  if ! includes=$("$python" "$(dirname "$0")/list_includes.py" \
                    "$compile_commands" "$source_under_test"); then
    echo "FAIL: could not determine the includes of $source_under_test" >&2
    status=1
    continue
  fi
  # A translation unit that reports no includes at all was not found in
  # compile_commands.json, or is not compiled any more - either way nothing was
  # verified, and reporting "ok" would be the vacuous pass again. Note this is
  # per-source rather than fatal for the run: one stale path should not stop the
  # other sources from being checked, since a partial answer plus a named failure
  # is more useful than no answer.
  if [ -z "$includes" ]; then
    echo "FAIL: no includes reported for $source_under_test - nothing was verified" >&2
    status=1
    continue
  fi
  matches=$(printf '%s\n' "$includes" | grep -F "$nanopb_real/" || true)
  if [ -n "$matches" ]; then
    echo "FAIL: $source_under_test includes Nanopb headers:" >&2
    printf '  %s\n' "$matches" >&2
    status=1
  else
    echo "ok: $source_under_test includes no Nanopb headers"
  fi
done

exit $status

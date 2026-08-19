#!/bin/sh
# Verify boomlink_linkframe_encode()'s compile-time output bound still rejects an
# undersized caller buffer - in whichever language it is invoked for.
#
# The encoder writes 20 bytes unconditionally and takes no capacity argument, so
# the array bound in its declaration is the only thing standing between a caller
# with a too-small buffer and a silent overflow. The case that matters is not the
# obvious one: a too-small standalone object is caught by AddressSanitizer at
# runtime, but writing past a short array INSIDE a larger object stays within a
# valid allocation, so ASan reports nothing at all. Only a compile-time bound
# sees it.
#
# The bound is spelled differently per language, so this script is run once per
# language with its own fixtures and its own expected diagnostic:
#
#   C   - `uint8_t out[static 20]`, diagnosed as -Wstringop-overflow (GCC) or
#         -Warray-bounds (clang). A WARNING by default, which is why the negative
#         compile below adds -Werror.
#   C++ - `[static N]` is not valid C++, so the header restores the bound with a
#         template wrapper whose static_assert fires instead.
#
# Both need testing, and for the same reason: neither is exercised by any target
# in either build. Every library and tool in this package is C and never passes
# an undersized buffer, and the C++ caller (the firmware's C++ radio layer) does
# not exist yet. So without this, both bounds could be deleted outright with the
# whole suite staying green - verified, for both languages.
#
# The negative direction is checked by REASON, not by exit status alone: "the
# compiler exited nonzero" is also what a missing compiler, a wrong -I path or an
# unrelated typo produce, and a negative test that passes for those reasons is
# worse than no test. It is also compiled at more than one optimization level,
# because the C bound's whole value rests on it not being an optimizer-dependent
# diagnostic - the header claims it fires at every level, and this is what makes
# that claim tested rather than asserted.
#
# Usage: check_encoder_bound.sh <compiler> <std-flag> <include-dir> \
#                                <ok-source> <too-small-source> <diagnostic-regex>
set -eu

if [ "$#" -ne 6 ]; then
  echo "usage: $0 <compiler> <std-flag> <include-dir> <ok-source> <too-small-source> <diagnostic-regex>" >&2
  exit 2
fi

compiler=$1
std_flag=$2
include_dir=$3
ok_source=$4
too_small_source=$5
diagnostic_regex=$6

# Optimization levels the negative case must be rejected at. -O0 is the
# interesting one: GCC's -Wstringop-overflow family is generally
# optimizer-driven, so a bound that only fired with optimization on would be
# absent from exactly the build a developer debugs in - and from the firmware's
# Debug build, which is what CI compiles.
OPT_LEVELS="-O0 -O2"

status=0

for source in "$ok_source" "$too_small_source"; do
  if [ ! -f "$source" ]; then
    echo "FAIL: $source does not exist - this check cannot verify anything" >&2
    exit 1
  fi
done

# A temporary output directory, so this test never writes into the source tree
# and two concurrent ctest runs cannot collide on an object file.
work=$(mktemp -d)
# shellcheck disable=SC2064 # $work must expand now, not when the trap fires.
trap "rm -rf '$work'" EXIT INT TERM

# 1. A correctly-sized buffer must compile, warning-free, at every level. -Werror
#    matters here too: a bound that produced a false positive on a LEGITIMATE
#    caller would be worse than no bound, since the workaround people reach for
#    is to delete it.
for opt in $OPT_LEVELS; do
  if ! ok_output=$("$compiler" "$std_flag" "$opt" -Wall -Wextra -Werror -I"$include_dir" \
                     -c "$ok_source" -o "$work/ok.o" 2>&1); then
    echo "FAIL: a correctly-sized caller no longer compiles at $opt:" >&2
    printf '%s\n' "$ok_output" >&2
    exit 1
  fi
done
echo "ok: a correctly-sized caller compiles clean at$(printf ' %s' $OPT_LEVELS)"

# 2. The call must reach the plain C symbol. In C this is trivially true; in C++
#    it is not - the template delegates through a namespaced `extern "C"`
#    declaration, and a version of it that forwarded to itself or to a
#    C++-mangled overload would still compile and still be bound-checked, while
#    no longer calling the implementation the firmware actually contains.
if ! symbols=$(nm "$work/ok.o" 2>&1); then
  echo "FAIL: cannot read symbols from the compiled object: $symbols" >&2
  exit 1
fi
if ! printf '%s\n' "$symbols" | grep -qE ' U boomlink_linkframe_encode$'; then
  echo "FAIL: the caller does not reference the unmangled C symbol" >&2
  echo "      boomlink_linkframe_encode - the call is not reaching the encoder." >&2
  printf '%s\n' "$symbols" >&2
  status=1
else
  echo "ok: the caller references the unmangled C encoder symbol"
fi

# 3. An undersized buffer must NOT compile, at any level, and must be rejected
#    for the RIGHT reason.
for opt in $OPT_LEVELS; do
  if bad_output=$("$compiler" "$std_flag" "$opt" -Wall -Wextra -Werror -I"$include_dir" \
                    -c "$too_small_source" -o "$work/bad.o" 2>&1); then
    echo "FAIL: an undersized output buffer compiled successfully at $opt." >&2
    echo "      $(basename "$too_small_source") writes past its buffer into the" >&2
    echo "      neighbouring struct member, where AddressSanitizer cannot see it." >&2
    printf '%s\n' "$bad_output" >&2
    exit 1
  fi
  if ! printf '%s\n' "$bad_output" | grep -qiE "$diagnostic_regex"; then
    echo "FAIL: the undersized caller was rejected at $opt, but NOT by the bound" >&2
    echo "      check (expected a diagnostic matching '$diagnostic_regex') - so this" >&2
    echo "      test would pass even with the bound removed:" >&2
    printf '%s\n' "$bad_output" >&2
    exit 1
  fi
done
echo "ok: an undersized output buffer is rejected by the bound check at$(printf ' %s' $OPT_LEVELS)"

exit $status

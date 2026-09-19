# Runs boomdetect_selftest_tool and diffs its output against the checked-in host
# capture. A script rather than a shell one-liner so the test works the same way
# wherever ctest runs, and so a mismatch prints the first differing line instead
# of "exit 1".
execute_process(COMMAND "${TOOL}" OUTPUT_VARIABLE out RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "boomdetect_selftest_tool exited ${rc}")
endif()

file(READ "${EXPECTED}" want_raw)
string(REPLACE "\n" ";" want_lines "${want_raw}")
string(REPLACE "\n" ";" got_lines "${out}")

# Comments and blanks are documentation, not data.
set(want "")
foreach(l IN LISTS want_lines)
  if(l MATCHES "^DST")
    list(APPEND want "${l}")
  endif()
endforeach()
set(got "")
foreach(l IN LISTS got_lines)
  if(l MATCHES "^DST")
    list(APPEND got "${l}")
  endif()
endforeach()

list(LENGTH want n_want)
list(LENGTH got n_got)

# DSTSIG first: it is the checksum of the generated INPUT, so a mismatch there
# means the two sides did not score the same signal and every downstream
# difference is a consequence rather than a finding of its own.
foreach(side want got)
  set(sig_${side} "")
  foreach(l IN LISTS ${side})
    if(l MATCHES "^DSTSIG")
      set(sig_${side} "${l}")
    endif()
  endforeach()
endforeach()
if(NOT sig_want STREQUAL sig_got)
  message(FATAL_ERROR
    "input signature differs, so nothing downstream is comparable:\n"
    "  expected ${sig_want}\n  got      ${sig_got}")
endif()

if(NOT n_want EQUAL n_got)
  message(FATAL_ERROR "expected ${n_want} DST lines, got ${n_got}")
endif()

math(EXPR last "${n_want} - 1")
foreach(i RANGE ${last})
  list(GET want ${i} w)
  list(GET got ${i} g)
  if(NOT w STREQUAL g)
    message(FATAL_ERROR
      "line ${i} differs. The arithmetic moved; see the header of\n"
      "${EXPECTED} for what that means.\n"
      "  expected ${w}\n  got      ${g}")
  endif()
endforeach()
message(STATUS "selftest: ${n_want} lines identical to the host fixture")

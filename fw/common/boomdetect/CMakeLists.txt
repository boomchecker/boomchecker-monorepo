# 3.21, not 3.14: CMakePresets.json's testPresets need it, same as
# fw/common/boomlink.
cmake_minimum_required(VERSION 3.21)

# Two ways in, exactly as fw/common/boomlink does it:
#  1. add_subdirectory()'d from fw/bom-stm32node - cross-compiled for the
#     Cortex-M33, no tests, no sanitizers, CMSIS Core headers supplied by the
#     firmware (see BOOMDETECT_CMSIS_CORE_DIR below);
#  2. configured standalone with the host compiler - builds the same library
#     natively plus the test tools, which is the whole point of this package
#     existing separately from the firmware.
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  project(boomdetect LANGUAGES C)
  set(CMAKE_C_STANDARD 11)
  set(CMAKE_C_STANDARD_REQUIRED ON)
  set(BOOMDETECT_STANDALONE TRUE)
else()
  set(BOOMDETECT_STANDALONE FALSE)
endif()

# Two INDEPENDENT axes, and conflating them was a real trap: standalone-vs-
# embedded decides whether this package owns the project and its tests,
# host-vs-cross decides whether CMSIS-DSP needs Core headers. A third consumer
# that add_subdirectory()s this package for a HOST build - which is exactly the
# shape a WAV/parity harness wants - is embedded but not cross, and keying the
# CMSIS branch off STANDALONE sent it into the cross path to fail on Core
# headers it does not need.
option(BOOMDETECT_HOST_BUILD "Build against a host libc rather than CMSIS Core" OFF)
if(BOOMDETECT_STANDALONE AND NOT CMAKE_CROSSCOMPILING)
  set(BOOMDETECT_HOST_BUILD ON)
endif()
if(NOT CMAKE_CROSSCOMPILING AND NOT BOOMDETECT_HOST_BUILD AND NOT BOOMDETECT_CMSIS_CORE_DIR)
  # Embedded in a host project and told nothing: the host path is the only one
  # that can work, so take it rather than failing on Core headers.
  set(BOOMDETECT_HOST_BUILD ON)
endif()

# Declared only in the standalone branch: neither is meaningful under the ARM
# cross build (no sanitizer runtime there, and firmware warning policy belongs
# in fw/bom-stm32node/cmake/gcc-arm-none-eabi.cmake), and declaring them anyway
# would leave two settable-but-inert knobs in the firmware's `cmake -LH`.
if(BOOMDETECT_STANDALONE)
  option(BOOMDETECT_SANITIZE "Build with AddressSanitizer + UndefinedBehaviorSanitizer" OFF)
  option(BOOMDETECT_WERROR "Treat warnings as errors in this package's own targets" OFF)
  # For a pipeline that wants to guarantee the suite is instrumented rather than
  # trust that a preset was used; a configure-time warning is invisible in a
  # green run.
  option(BOOMDETECT_REQUIRE_SANITIZERS "Fail configuration if BOOMDETECT_SANITIZE is OFF" OFF)

  # MinGW-w64 ships no libasan/libubsan, so the Debug preset cannot instrument
  # there. Rather than fail the configure - which would leave a Windows
  # developer with no host suite at all - build uninstrumented and say so.
  # REQUIRE_SANITIZERS still fails, since a pipeline asking for instrumentation
  # should not get a quiet downgrade.
  if(BOOMDETECT_SANITIZE AND MINGW)
    if(BOOMDETECT_REQUIRE_SANITIZERS)
      message(FATAL_ERROR
        "BOOMDETECT_REQUIRE_SANITIZERS is ON but MinGW has no sanitizer runtime.")
    endif()
    message(WARNING
      "BOOMDETECT_SANITIZE is ON but this is MinGW, which ships no ASan/UBSan "
      "runtime: building the suite uninstrumented. The instrumented run is what "
      "CI does on Linux; do not read a green run here as proof of memory safety.")
    set(BOOMDETECT_SANITIZE OFF)
  endif()

  if(BOOMDETECT_SANITIZE)
    # Directory scope, so the fetched CMSIS-DSP is instrumented too. That is
    # deliberate: this package indexes float arrays by counts that come out of
    # model headers, and the MFCC scratch buffer has a documented sizing trap
    # (see boomdetect_mfcc_f32.c). Those are exactly the bugs a sanitizer
    # catches and a pass/fail exit code does not.
    add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=all -g -O1)
    add_link_options(-fsanitize=address,undefined)
  elseif(BOOMDETECT_REQUIRE_SANITIZERS)
    message(FATAL_ERROR
      "BOOMDETECT_REQUIRE_SANITIZERS is ON but BOOMDETECT_SANITIZE is OFF. "
      "Configure with `cmake --preset Debug`, or pass -DBOOMDETECT_SANITIZE=ON.")
  else()
    # Warn at CONFIGURE time. ctest hides output on a passing test, so on the
    # all-green run - the one that gives false confidence - nothing else would
    # say the suite ran uninstrumented.
    message(WARNING
      "BOOMDETECT_SANITIZE is OFF: this package's tests exercise float array "
      "indexing driven by model-header dimensions and an MFCC scratch buffer with "
      "a known sizing trap; without ASan/UBSan an out-of-bounds read passes green. "
      "Configure with `cmake --preset Debug` unless you specifically want the "
      "faster uninstrumented build.")
  endif()
endif()

set(BOOMDETECT_WARNING_FLAGS -Wall -Wextra)

# MinGW's default printf is msvcrt's, whose format checker does not know %zu, so
# every size_t in a test message is a -Wformat error under -Werror. The ANSI
# stdio shim gives the C99 conversions; directory scope because the tests are
# the ones that print, and they are several targets.
if(MINGW)
  add_compile_definitions(__USE_MINGW_ANSI_STDIO=1)
endif()

# Kept separate from the warning flags and guarded on STANDALONE as well as on
# the option: the option only exists in that branch, so a bare
# if(BOOMDETECT_WERROR) would read an uninitialised cache entry under the cross
# build and a -D on the firmware command line would take effect with nothing in
# `cmake -LH` to discover it from.
set(BOOMDETECT_WERROR_FLAGS "")
if(BOOMDETECT_STANDALONE AND BOOMDETECT_WERROR)
  set(BOOMDETECT_WERROR_FLAGS -Werror)
endif()

# --- CMSIS-DSP ---------------------------------------------------------------
#
# Fetched, not vendored. The copy this replaces was 467 files and 11 MB in the
# firmware tree with no LICENSE and no record of which release it came from -
# it read as v1.15.0 only because a helper script on the research branch pinned
# that tag.
#
# It lives HERE rather than in the firmware because this package needs it on the
# host too: without a host build of the MFCC there is no parity suite, and
# without a parity suite there is no reason for this package to exist.
include(FetchContent)

# One place, so the CI cache key and the upstream-drift check below cannot drift
# from what is actually fetched.
set(BOOMDETECT_CMSISDSP_TAG "v1.15.0")
set(BOOMDETECT_CMSISDSP_COMMIT "8f8bb8d56b346274c9b2918928f2d5c3dae51758")

# Upstream defaults LOOPUNROLL to ON; the hand-rolled build this replaces never
# defined ARM_MATH_LOOPUNROLL. Unrolling changes accumulation order in
# arm_dot_prod_f32/arm_rms_f32, which moves MFCC coefficients in the last
# places - and the deployed model was selected against the un-unrolled numbers.
# Turn it on only together with a re-measured hardware baseline.
#
# These three are upstream CMSIS-DSP's own option names, unprefixed, and setting
# them CACHE...FORCE stamps them on the CONSUMER's cache - the firmware's, shared
# with boomlink and everything else. That collision risk is accepted knowingly:
# FetchContent gives a subproject no private cache, upstream reads them via
# option()/if() at its own configure time, and renaming them would mean patching
# the fetched tree, which is the mechanism this package deliberately avoids.
# FORCE also means a -DLOOPUNROLL=ON on the command line is discarded in
# silence, which is intended here and checked for below.
set(LOOPUNROLL OFF CACHE BOOL "" FORCE)

if(BOOMDETECT_HOST_BUILD)
  # HOST=ON makes configDsp.cmake define __GNUC_PYTHON__, which is what lets
  # arm_math_types.h skip its #include of cmsis_compiler.h - a CMSIS *Core*
  # header that ships with the device support package, not with CMSIS-DSP, and
  # so does not exist on an x86 host at all.
  set(HOST ON CACHE BOOL "" FORCE)
else()
  # Cross build: the Core headers do exist, but only the firmware knows where.
  # It injects the path, so this package never reaches upward into a specific
  # board's tree - the same direction of dependency App/link/ has on
  # fw/common/boomlink.
  if(NOT BOOMDETECT_CMSIS_CORE_DIR)
    message(FATAL_ERROR
      "boomdetect is cross-compiling without BOOMDETECT_CMSIS_CORE_DIR. "
      "CMSIS-DSP includes cmsis_compiler.h from CMSIS Core, which ships with the "
      "device support package, not with DSP. Set it to your Drivers/CMSIS before "
      "the add_subdirectory() call.")
  endif()
  set(CMSISCORE "${BOOMDETECT_CMSIS_CORE_DIR}" CACHE PATH "" FORCE)
endif()

FetchContent_Declare(cmsisdsp
  GIT_REPOSITORY https://github.com/ARM-software/CMSIS-DSP.git
  # The tag, so the clone can stay shallow: fetching a bare SHA needs the full
  # history, which is minutes of CI on every firmware PR. A tag is a movable
  # ref, so it is NOT trusted - the commit it resolved to is verified after
  # populate, below, together with the content hashes.
  GIT_TAG        ${BOOMDETECT_CMSISDSP_TAG}
  GIT_SHALLOW    TRUE
  EXCLUDE_FROM_ALL TRUE
)
FetchContent_MakeAvailable(cmsisdsp)

# The single load-bearing numerical setting in this build, and it fails open:
# if upstream ever renames the option, option() simply defaults it back ON and
# every MFCC coefficient moves, with only a hardware fixture to notice. Check
# that the FORCE above actually took.
if(LOOPUNROLL)
  message(FATAL_ERROR
    "LOOPUNROLL is ON after fetching CMSIS-DSP. Unrolling changes accumulation "
    "order in arm_dot_prod_f32/arm_rms_f32, and the deployed model was selected "
    "against the un-unrolled numbers. Upstream may have renamed the option.")
endif()

# src/boomdetect_mfcc_f32.c is a hand-modified copy of ONE upstream file, and
# nothing binds the copy to the version fetched above. Bump the pin and the copy
# still compiles - against a possibly reordered arm_mfcc_instance_f32, a changed
# RFFT packing convention, a renamed helper - and produces different numbers on
# every platform, with no test anywhere that can fail. These hashes turn that
# into a configure error, so a bump becomes a review of the diff rather than a
# silent regression. They are the file our copy was derived from and the header
# whose types it uses; update both only together with a re-measured baseline.
# The tag is a movable ref. Check what it actually resolved to; the tag being
# re-pointed at a different commit is the failure mode a version string cannot
# see, and it is precisely the class of unrecorded drift that cost this project
# a day already. Non-fatal if git is unavailable: the content hashes below are
# the load-bearing check, this one only names the cause faster.
find_program(BOOMDETECT_GIT git)
if(BOOMDETECT_GIT)
  execute_process(
    COMMAND "${BOOMDETECT_GIT}" -c safe.directory=* rev-parse HEAD
    WORKING_DIRECTORY "${cmsisdsp_SOURCE_DIR}"
    OUTPUT_VARIABLE BOOMDETECT_CMSISDSP_GOT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET RESULT_VARIABLE BOOMDETECT_GIT_RC)
  if(BOOMDETECT_GIT_RC EQUAL 0 AND
     NOT BOOMDETECT_CMSISDSP_GOT STREQUAL BOOMDETECT_CMSISDSP_COMMIT)
    message(FATAL_ERROR
      "CMSIS-DSP ${BOOMDETECT_CMSISDSP_TAG} resolved to ${BOOMDETECT_CMSISDSP_GOT}, "
      "not the pinned ${BOOMDETECT_CMSISDSP_COMMIT}. The tag moved.")
  endif()
endif()

# Two parallel lists rather than a list of pairs: a ";" inside a set() argument
# is a list separator, so "a;b" "c;d" is four elements, not two.
set(BOOMDETECT_UPSTREAM_PATHS
  "Source/TransformFunctions/arm_mfcc_f32.c"
  "Include/dsp/transform_functions.h")
set(BOOMDETECT_UPSTREAM_HASHES
  "3f4487c75ff7ab4ae79e1ceec2b7973f30fbd2dfbe43d9e5646b332deb212fb0"
  "6ab56f4342e6bf56f550af9d0ea80bcfe5773ad8bd1c4b9e580238a311226fb6")

list(LENGTH BOOMDETECT_UPSTREAM_PATHS _n)
math(EXPR _last "${_n} - 1")
foreach(_i RANGE ${_last})
  list(GET BOOMDETECT_UPSTREAM_PATHS ${_i} _rel)
  list(GET BOOMDETECT_UPSTREAM_HASHES ${_i} _want)
  set(_path "${cmsisdsp_SOURCE_DIR}/${_rel}")
  if(NOT EXISTS "${_path}")
    message(FATAL_ERROR
      "CMSIS-DSP ${BOOMDETECT_CMSISDSP_TAG} does not contain ${_rel}. "
      "src/boomdetect_mfcc_f32.c is derived from it; the pin moved under us.")
  endif()
  # Hash the CONTENT with line endings normalised, not the file: a Windows git
  # with core.autocrlf=true checks the fetched tree out with CRLF, and file(SHA256)
  # then reports a drift that is not one. The two files are text, so reading
  # them into a string is safe; CMake's file(READ) keeps the bytes as they are.
  file(READ "${_path}" _content)
  string(REPLACE "\r\n" "\n" _content "${_content}")
  string(SHA256 _got "${_content}")
  if(NOT _got STREQUAL _want)
    message(FATAL_ERROR
      "Upstream ${_rel} changed.\n"
      "  expected ${_want}\n"
      "  got      ${_got}\n"
      "src/boomdetect_mfcc_f32.c is a modified copy of that file (RMS rather "
      "than absolute-maximum conditioning of the FFT input) and the deployed "
      "model was selected against what it produces. Diff the two, port the "
      "deviation deliberately, re-measure the fixture on hardware, then update "
      "the hash here.")
  endif()
endforeach()

# --- this package ------------------------------------------------------------

# Split so the registry is genuinely replaceable rather than only claimed to be:
# the pipeline objects reference the four classifier_* accessors and nothing
# else, so linking a different definition of them swaps the whole model table.
# tests/stub_registry.c does that, which is what turns the claim into a check.
add_library(boomdetect_core OBJECT
  src/boomdetect.c
  src/boomdetect_alarm.c
  src/extractor_stats.c
  src/extractor_spectral.c
  src/extractor_logmel.c
  src/frame_scalars.c
  src/nn_infer.c
  src/boomdetect_mfcc_f32.c
  src/boomdetect_selftest.c
  src/mfcc_processor.c
)

# One translation unit per model. `bdtrain export` generates model_<name>.c for
# a trained model; adding it here, to models/models.h and to
# src/classifier_registry.c is what puts it in the image (see
# training/README.md, "Adding a model to the board").
set(BOOMDETECT_MODEL_SOURCES
  models/model_mlp_v6.c
  models/model_svm_v3.c
  models/model_svm_l2.c
  models/model_gbt_l2.c
  models/model_mlp_l2.c
  models/model_cnn_1d.c
)

add_library(boomdetect STATIC
  src/classifier_registry.c
  ${BOOMDETECT_MODEL_SOURCES}
  $<TARGET_OBJECTS:boomdetect_core>
)
# PUBLIC is the API: boomdetect.h, classifier.h, dsp_config.h. The MFCC wrapper
# and its generated tables are internals and live in src/ - published, they put
# ~10 KB of `static const` tables on the firmware's include path for any
# translation unit to duplicate.
target_include_directories(boomdetect
  PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
          ${CMAKE_CURRENT_SOURCE_DIR}/models
)
target_link_libraries(boomdetect PUBLIC CMSISDSP)

# The object library needs the same headers and the same flags; it does not link
# anything itself, so the usage requirements are copied rather than inherited.
target_include_directories(boomdetect_core
  PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
          ${CMAKE_CURRENT_SOURCE_DIR}/models
)
target_link_libraries(boomdetect_core PUBLIC CMSISDSP)

# SYSTEM, so a warning inside arm_math.h does not fail our build under -Werror.
get_target_property(BOOMDETECT_CMSIS_INC CMSISDSP INTERFACE_INCLUDE_DIRECTORIES)
if(BOOMDETECT_CMSIS_INC)
  target_include_directories(CMSISDSP SYSTEM INTERFACE ${BOOMDETECT_CMSIS_INC})
endif()

# libm, host only. arm_vlog_f32() calls logf(), and upstream's CMSISDSP target
# does not link a math library because on a bare-metal target there is no
# separate one to link - newlib folds it into libc. On glibc it is a separate
# DSO, so without this the host build compiles cleanly and then fails at link
# with "undefined reference to logf".
if(BOOMDETECT_STANDALONE)
  target_link_libraries(boomdetect PUBLIC m)
endif()
target_compile_options(boomdetect PRIVATE
  ${BOOMDETECT_WARNING_FLAGS} ${BOOMDETECT_WERROR_FLAGS})

# The firmware compiled its CMSIS-DSP translation units at -O2 even in Debug
# (see fw/bom-stm32node/CMakeLists.txt): the MFCC shares a 21.33 ms real-time
# budget with the PDM conversion, and the Debug toolchain is -O0 -g3. That was a
# set_source_files_properties() at firmware directory scope, which cannot reach
# a target defined here - so it has to be re-applied, or the detector silently
# stops keeping up with the microphone in Debug builds.
#
# Cross build only: standalone, the sanitizer preset wants its own -O1 and the
# tests are not real-time.
if(NOT BOOMDETECT_STANDALONE)
  target_compile_options(boomdetect PRIVATE -O2)
  target_compile_options(boomdetect_core PRIVATE -O2)
  target_compile_options(CMSISDSP PRIVATE -O2)

  # And assert it took. "-O2 did not survive the move" is one of the four causes
  # the fixture's own header tells a reader to suspect, and until now the only
  # thing that could notice was a board. A property read is not a compile, but it
  # catches the realistic failure - a target renamed, a branch restructured, this
  # block quietly not applying to something - at configure time and for free.
  foreach(_t boomdetect boomdetect_core CMSISDSP)
    get_target_property(_opts ${_t} COMPILE_OPTIONS)
    if(NOT _opts MATCHES "-O2")
      message(FATAL_ERROR
        "${_t} is missing -O2 on the cross build. The MFCC shares a 21.33 ms "
        "real-time budget with the PDM conversion and the Debug toolchain is "
        "-O0 -g3; without this the detector stops keeping up with the microphone.")
    endif()
  endforeach()
endif()

if(BOOMDETECT_STANDALONE)
  enable_testing()

  # Proves the harness can fail. bd_test.h's `return 1` is the only way any C
  # test here can fail and it only runs when a test is already failing, so
  # nothing else exercises it - see tests/check_bd_test.sh.
  add_executable(boomdetect_bd_test_selfcheck tests/bd_test_selfcheck.c)
  target_include_directories(boomdetect_bd_test_selfcheck PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  target_compile_options(boomdetect_bd_test_selfcheck PRIVATE
    ${BOOMDETECT_WARNING_FLAGS} ${BOOMDETECT_WERROR_FLAGS})
  # Through an explicit interpreter: ctest cannot exec a .sh directly on Windows
  # (BAD_COMMAND), and Git for Windows puts a POSIX sh on the PATH. If there is
  # none the test is left out with a warning rather than registered to fail.
  find_program(BOOMDETECT_SH NAMES sh bash)
  if(BOOMDETECT_SH)
    add_test(NAME boomdetect_test_harness
      COMMAND ${BOOMDETECT_SH} ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_bd_test.sh
              $<TARGET_FILE:boomdetect_bd_test_selfcheck>)
  else()
    message(WARNING
      "no POSIX sh found: boomdetect_test_harness (the check that the test "
      "harness can fail) is not registered on this host.")
  endif()

  # The fixture generator as an executable, which is what lets a host reproduce
  # tests/vectors/ without a board, and what a future parity harness drives.
  add_executable(boomdetect_selftest_tool tests/selftest_tool.c)
  target_link_libraries(boomdetect_selftest_tool PRIVATE boomdetect)
  target_compile_options(boomdetect_selftest_tool PRIVATE
    ${BOOMDETECT_WARNING_FLAGS} ${BOOMDETECT_WERROR_FLAGS})

  # Host regression: the tool's own output against a checked-in host capture.
  # Deliberately a DIFFERENT file from the board fixture. Both run identical C,
  # but an x86 host and a Cortex-M33 do not have to produce identical float32
  # bits - different instruction selection, and FMA contraction on one and not
  # the other - so asserting they match would be asserting something nobody
  # measured. check_selftest.py compares the board against its own fixture; this
  # compares the host against its own. Whether the two agree is a finding either
  # way, and tests/vectors/README.md records what was actually observed.
  #
  # The bit-exact claim holds for the toolchain that recorded the file (the
  # Linux CI compiler). MinGW-w64 at -O2 is the same C with a different
  # instruction selection and lands a few ULP away - measured 4.1e-7 relative
  # on the worst feature, 6.8e-7 on a decision, see the fixture's header - so
  # there the tool compares within a tolerance an order of magnitude above that
  # measurement. Anywhere else the comparison stays exact.
  if(MINGW)
    add_test(NAME boomdetect_selftest_host
      COMMAND boomdetect_selftest_tool
              --compare ${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/selftest_host.txt
              --rel 1e-5)
  else()
    add_test(NAME boomdetect_selftest_host
      COMMAND ${CMAKE_COMMAND}
              -DTOOL=$<TARGET_FILE:boomdetect_selftest_tool>
              -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/selftest_host.txt
              -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_selftest_host.cmake)
  endif()

  # Links the pipeline against a one-model fake registry, with neither
  # classifier_registry.c nor the model translation units present.
  add_executable(boomdetect_stub_registry_test
    tests/stub_registry_test.c
    tests/stub_registry.c
    $<TARGET_OBJECTS:boomdetect_core>)
  target_link_libraries(boomdetect_stub_registry_test PRIVATE CMSISDSP m)
  target_include_directories(boomdetect_stub_registry_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  target_compile_options(boomdetect_stub_registry_test PRIVATE
    ${BOOMDETECT_WARNING_FLAGS} ${BOOMDETECT_WERROR_FLAGS})
  add_test(NAME boomdetect_stub_registry COMMAND boomdetect_stub_registry_test)

  # The items are source stems minus "_test": tests/registry_test.c builds
  # boomdetect_registry_test and registers as the ctest `boomdetect_registry`.
  # model_parity reads tests/vectors/parity_vectors.h, which `bdtrain export`
  # regenerates whenever a model is exported; it holds every registered model's
  # C forward pass to the Python one.
  foreach(test IN ITEMS mfcc_smoke registry pipeline alarm nn_infer extractor model_parity)
    add_executable(boomdetect_${test}_test tests/${test}_test.c)
    target_link_libraries(boomdetect_${test}_test PRIVATE boomdetect)
    # tests/ for the harness, src/ because these are white-box tests: the MFCC
    # wrapper is an internal, and a test that could only reach the public API
    # could not check it.
    target_include_directories(boomdetect_${test}_test PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/tests
      ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(boomdetect_${test}_test PRIVATE
      ${BOOMDETECT_WARNING_FLAGS} ${BOOMDETECT_WERROR_FLAGS})
    add_test(NAME boomdetect_${test} COMMAND boomdetect_${test}_test)
  endforeach()
endif()

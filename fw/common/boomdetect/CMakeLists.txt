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

# Declared only in the standalone branch: neither is meaningful under the ARM
# cross build (no sanitizer runtime there, and firmware warning policy belongs
# in fw/bom-stm32node/cmake/gcc-arm-none-eabi.cmake), and declaring them anyway
# would leave two settable-but-inert knobs in the firmware's `cmake -LH`.
if(BOOMDETECT_STANDALONE)
  option(BOOMDETECT_SANITIZE "Build with AddressSanitizer + UndefinedBehaviorSanitizer" OFF)
  option(BOOMDETECT_WERROR "Treat warnings as errors in this package's own targets" OFF)

  if(BOOMDETECT_SANITIZE)
    # Directory scope, so the fetched CMSIS-DSP is instrumented too. That is
    # deliberate: this package indexes float arrays by counts that come out of
    # model headers, and the MFCC scratch buffer has a documented sizing trap
    # (see boomdetect_mfcc_f32.c). Those are exactly the bugs a sanitizer
    # catches and a pass/fail exit code does not.
    add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=all -g -O1)
    add_link_options(-fsanitize=address,undefined)
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

# Upstream defaults LOOPUNROLL to ON; the hand-rolled build this replaces never
# defined ARM_MATH_LOOPUNROLL. Unrolling changes accumulation order in
# arm_dot_prod_f32/arm_rms_f32, which moves MFCC coefficients in the last
# places - and the deployed model was selected against the un-unrolled numbers.
# Turn it on only together with a re-measured hardware baseline.
set(LOOPUNROLL OFF CACHE BOOL "" FORCE)

if(BOOMDETECT_STANDALONE)
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
      "boomdetect was add_subdirectory()'d without BOOMDETECT_CMSIS_CORE_DIR. "
      "CMSIS-DSP includes cmsis_compiler.h from CMSIS Core, which ships with the "
      "device support package, not with DSP. Set it to your Drivers/CMSIS before "
      "the add_subdirectory() call.")
  endif()
  set(CMSISCORE "${BOOMDETECT_CMSIS_CORE_DIR}" CACHE PATH "" FORCE)
endif()

FetchContent_Declare(cmsisdsp
  GIT_REPOSITORY https://github.com/ARM-software/CMSIS-DSP.git
  GIT_TAG        v1.15.0
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(cmsisdsp)

# --- this package ------------------------------------------------------------

add_library(boomdetect STATIC
  src/boomdetect_mfcc_f32.c
  src/mfcc_processor.c
  src/svm_classifier.c
)
target_include_directories(boomdetect PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  # models/ is PUBLIC only because svm_classifier.c picks the active model with
  # an #include; once the classifier registry lands, each model is its own
  # translation unit and this can go PRIVATE.
  ${CMAKE_CURRENT_SOURCE_DIR}/models
)
target_link_libraries(boomdetect PUBLIC CMSISDSP)

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
  target_compile_options(CMSISDSP PRIVATE -O2)
endif()

if(BOOMDETECT_STANDALONE)
  enable_testing()

  add_executable(boomdetect_mfcc_smoke tests/mfcc_smoke_test.c)
  target_link_libraries(boomdetect_mfcc_smoke PRIVATE boomdetect)
  target_compile_options(boomdetect_mfcc_smoke PRIVATE
    ${BOOMDETECT_WARNING_FLAGS} ${BOOMDETECT_WERROR_FLAGS})
  add_test(NAME boomdetect_mfcc_smoke COMMAND boomdetect_mfcc_smoke)
endif()

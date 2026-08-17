# 3.21, not 3.20: CMakePresets.json's testPresets (used by `ctest --preset`)
# need 3.21 - a lower cmake_minimum_required here would understate what this
# file actually needs to be usable end-to-end.
cmake_minimum_required(VERSION 3.21)

# This file is consumed two different ways:
#
# 1. add_subdirectory()'d from fw/bom-stm32node/CMakeLists.txt, under that
#    project's ARM cross toolchain - produces only the `boomlink_protocol`
#    static library (generated Nanopb codec + boomlink_codec.c), linked into
#    the firmware. No tests are built this way: cross-compiled host tooling
#    makes no sense, and the STM32 target has no way to run pytest.
# 2. configured standalone, with the host's native compiler - additionally
#    builds `boomlink_codec_tool` (a small CLI used by both C and Python
#    tests) and registers the native/host test suite via CTest. This is what
#    `cmake --preset Debug` in this directory does (see CMakePresets.json).
#
# boomlink.md section 4 requires the .proto files to not live only inside the
# STM32 target; this dual-mode file is how that requirement is met without
# maintaining the Nanopb-generation logic twice.
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  project(boomlink_protocol LANGUAGES C)
  set(CMAKE_C_STANDARD 11)
  set(CMAKE_C_STANDARD_REQUIRED ON)
  set(BOOMLINK_STANDALONE TRUE)
else()
  set(BOOMLINK_STANDALONE FALSE)
endif()

# ASan+UBSan for the host test suite - this package's whole reason to exist
# as a host-testable build is exercising a wire-format parser on
# hostile/malformed bytes (see tests/test_compatibility.py); a memory-safety
# bug in that parser (or in the C tool driving it) is exactly what a
# sanitizer catches and a plain pass/fail exit code does not. Host-only:
# never applied to the ARM cross build, which has no sanitizer runtime.
option(BOOMLINK_SANITIZE "Build with AddressSanitizer + UndefinedBehaviorSanitizer" OFF)
if(BOOMLINK_STANDALONE AND BOOMLINK_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=all -g -O1)
  add_link_options(-fsanitize=address,undefined)
endif()

# -Wall -Wextra for the host build only (the ARM cross build already
# inherits -Wall from fw/bom-stm32node/cmake/gcc-arm-none-eabi.cmake, so
# without this the host build - the only one that compiles the CLI tool
# doing hand-rolled buffer/argument-parsing work in tests/codec_tool.c -
# was the weaker of the two on a package whose test suite exists
# specifically to catch C-safety bugs. Verified this produces zero warnings
# against the current source before enabling it unconditionally.
if(BOOMLINK_STANDALONE)
  add_compile_options(-Wall -Wextra)
endif()

# Nanopb is vendored once, under bom-stm32node/third_party/ (git submodule
# pinned to release 0.4.9), and referenced from here by relative path rather
# than duplicating the submodule: both this host-native build and the ARM
# firmware build need the exact same runtime/generator. A CACHE variable
# (not a plain `set`) so this package can be relocated or paired with a
# differently-laid-out sibling without editing this file - e.g. a future
# host-only consumer that doesn't live under fw/bom-stm32node at all.
# Namespaced BOOMLINK_NANOPB_DIR, not a bare NANOPB_DIR: a CACHE variable
# with a generic name is a real collision risk the moment anything else in
# a parent build sets its own same-named NANOPB_DIR before this file's
# add_subdirectory() runs - CACHE variables are first-write-wins, so this
# package would either silently build against the wrong (and unpinned)
# path, or fail loudly if that path doesn't contain a Nanopb checkout.
set(BOOMLINK_NANOPB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../bom-stm32node/third_party/nanopb
    CACHE PATH "Path to the vendored Nanopb submodule")
set(BOOMLINK_NANOPB_GENERATOR ${BOOMLINK_NANOPB_DIR}/generator/nanopb_generator.py)

if(NOT EXISTS ${BOOMLINK_NANOPB_GENERATOR})
  message(FATAL_ERROR
    "Nanopb submodule not found at ${BOOMLINK_NANOPB_DIR}. Run: "
    "git submodule update --init fw/bom-stm32node/third_party/nanopb")
endif()

# Named boomlink_nanopb (not the generic "nanopb" nanopb's own vendored
# CMakeLists.txt would use) since this package is designed to be
# add_subdirectory()'d into other builds - a bare "nanopb" target risks a
# hard `add_library cannot create target "nanopb" because another target
# with that name already exists` the moment any sibling package vendors
# Nanopb the same way, or itself add_subdirectory()'s the real upstream
# CMakeLists.txt. Guarded against exactly that: a consumer that already
# defines its own target literally named boomlink_nanopb (however unlikely)
# is trusted instead of fought.
#
# The library and the alias are guarded SEPARATELY, not as one block: if a
# consumer's own boomlink_nanopb already exists but boomlink::nanopb does
# not, only wrapping the whole thing in a single
# `if(NOT TARGET boomlink_nanopb)` would skip creating the alias too,
# leaving target_link_libraries(boomlink_protocol PUBLIC boomlink::nanopb)
# below referencing a target that was never created - a new failure this
# guard would have introduced instead of prevented.
if(NOT TARGET boomlink_nanopb)
  add_library(boomlink_nanopb STATIC
    ${BOOMLINK_NANOPB_DIR}/pb_common.c
    ${BOOMLINK_NANOPB_DIR}/pb_encode.c
    ${BOOMLINK_NANOPB_DIR}/pb_decode.c
  )
  target_include_directories(boomlink_nanopb PUBLIC ${BOOMLINK_NANOPB_DIR})
endif()
if(NOT TARGET boomlink::nanopb)
  add_library(boomlink::nanopb ALIAS boomlink_nanopb)
endif()

# The Nanopb generator shells out to protoc (via grpcio-tools' bundled copy,
# falling back to a system install) and needs the `protobuf` package to parse
# what protoc produces - see fw/bom-stm32node/third_party/nanopb/requirements.txt.
# Checked at configure time so a missing dependency is one clear error instead
# of a confusing generator stack trace mid-build.
#
# Code generation always runs on the build host, never the target - true
# even under fw/bom-stm32node's ARM cross build. Today's toolchain files
# (fw/bom-stm32node/cmake/*.cmake) don't set CMAKE_FIND_ROOT_PATH, so this
# has never actually mattered, but say so explicitly rather than relying on
# that being an accident of the current toolchain files: a Python
# interpreter never lives in an embedded sysroot. Scoped to this directory
# only (no PARENT_SCOPE), so it can't affect find_program() calls made by
# whatever included this file.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
execute_process(
  COMMAND ${Python3_EXECUTABLE} -c "import google.protobuf, grpc_tools.protoc"
  RESULT_VARIABLE BOOMLINK_PYTHON_DEPS_MISSING
  OUTPUT_QUIET ERROR_QUIET
)
if(BOOMLINK_PYTHON_DEPS_MISSING)
  message(FATAL_ERROR
    "Python packages 'protobuf' and 'grpcio-tools' are required to generate "
    "Nanopb code from fw/common/boomlink/proto/*.proto. Install them "
    "(matching fw/common/boomlink/requirements.txt) with:\n"
    "    pip install -r requirements.txt\n"
    "or run `task setup` in fw/common/boomlink.")
endif()

set(BOOMLINK_PROTO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/proto)
set(BOOMLINK_NANOPB_OPTIONS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/nanopb)
set(BOOMLINK_PROTO_NAMES common header system envelope)
list(TRANSFORM BOOMLINK_PROTO_NAMES PREPEND ${BOOMLINK_PROTO_DIR}/ OUTPUT_VARIABLE BOOMLINK_PROTO_FILES)
list(TRANSFORM BOOMLINK_PROTO_FILES APPEND .proto)

# Every *.options file in nanopb/ (today, just system.options - see below),
# not hardcoded to that one name: CONFIGURE_DEPENDS means CMake reconfigures
# (and this glob re-runs) if one is added, removed, or renamed, not just
# when an already-known one's contents change. Without this, adding a new
# nanopb/<name>.options for a future .proto per this file's own pattern (and
# README.md's "Adding a new message group" instructions) would silently
# never be tracked: the .options file could be edited any number of times
# after that and the generator would keep using whatever bounds it read the
# first time a full clean build happened to run.
file(GLOB BOOMLINK_NANOPB_OPTIONS_FILES CONFIGURE_DEPENDS ${BOOMLINK_NANOPB_OPTIONS_DIR}/*.options)

set(BOOMLINK_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${BOOMLINK_GENERATED_DIR})

set(BOOMLINK_GENERATED_SOURCES "")
foreach(proto_name IN LISTS BOOMLINK_PROTO_NAMES)
  set(out_c ${BOOMLINK_GENERATED_DIR}/${proto_name}.pb.c)
  set(out_h ${BOOMLINK_GENERATED_DIR}/${proto_name}.pb.h)
  # Every generated file depends on all four .proto sources and all
  # nanopb/*.options files (not just its own), rather than tracking
  # envelope.proto's imports or each proto's own single options file
  # precisely: simpler, and the cost of an occasional unnecessary
  # regeneration of these four tiny files is not worth the extra
  # bookkeeping.
  #
  # No explicit -f<options file>: passing one global options file to every
  # invocation made the generator warn "did not match any fields" on the
  # three proto files with no matching options entry, every single build -
  # noise that buries the one thing that warning is supposed to catch (a
  # genuinely stale/typo'd entry). Nanopb auto-discovers a same-named
  # `<proto>.options` via its `-I`/`--options-path` search list instead
  # (verified against the vendored generator source): only system.proto has
  # one (nanopb/system.options, matched via the extra -I below), so the
  # other three simply find nothing and stay silent.
  add_custom_command(
    OUTPUT ${out_c} ${out_h}
    COMMAND ${Python3_EXECUTABLE} ${BOOMLINK_NANOPB_GENERATOR}
            -I${BOOMLINK_PROTO_DIR}
            -I${BOOMLINK_NANOPB_OPTIONS_DIR}
            -D${BOOMLINK_GENERATED_DIR}
            ${BOOMLINK_PROTO_DIR}/${proto_name}.proto
    DEPENDS ${BOOMLINK_PROTO_FILES} ${BOOMLINK_NANOPB_OPTIONS_FILES} ${BOOMLINK_NANOPB_GENERATOR}
    COMMENT "Generating Nanopb code for ${proto_name}.proto"
    VERBATIM
  )
  list(APPEND BOOMLINK_GENERATED_SOURCES ${out_c})
endforeach()

add_custom_target(boomlink_proto_gen DEPENDS ${BOOMLINK_GENERATED_SOURCES})

# NOT fully isolated the way PR 1's radio_layer isolates RadioLib behind a
# PRIVATE link (fw/bom-stm32node/CMakeLists.txt) - that isolation is
# enforced by the compiler there (consuming code can't even see RadioLib's
# headers). Here it can't be: boomlink_codec.h itself includes
# envelope.pb.h, which includes Nanopb's <pb.h>, so that header is
# necessarily on every consumer's include path via this PUBLIC link below.
# What IS true, and worth keeping true, is that consuming code has no
# reason to include Nanopb's headers or call pb_encode/pb_decode directly -
# boomlink_codec.h's two functions are the whole intended surface - just
# that nothing stops it at compile time the way radio_layer's boundary does.
add_library(boomlink_protocol STATIC
  ${BOOMLINK_GENERATED_SOURCES}
  ${CMAKE_CURRENT_SOURCE_DIR}/boomlink_codec.c
)
add_dependencies(boomlink_protocol boomlink_proto_gen)
target_include_directories(boomlink_protocol PUBLIC
  ${BOOMLINK_GENERATED_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(boomlink_protocol PUBLIC boomlink::nanopb)
# boomlink_codec.c uses _Static_assert (C11). Self-contained via
# target_compile_features rather than relying on whichever CMAKE_C_STANDARD
# happens to be set in the including scope: the standalone branch above sets
# it explicitly, but fw/bom-stm32node/CMakeLists.txt (a CubeMX-generated
# file - see its own header comment) is exactly the kind of file that gets
# silently regenerated, and this target should not depend on it staying at
# C11 by accident.
target_compile_features(boomlink_protocol PUBLIC c_std_11)

if(BOOMLINK_STANDALONE)
  enable_testing()

  add_executable(boomlink_codec_tool tests/codec_tool.c)
  target_link_libraries(boomlink_codec_tool PRIVATE boomlink_protocol)

  add_test(NAME boomlink_codec_selftest COMMAND boomlink_codec_tool selftest)

  # Host-side Python classes for the SAME .proto files, generated straight by
  # protoc (not through the Nanopb generator) so tests/*.py can act as an
  # independent, non-Nanopb implementation to cross-check against - never
  # committed, per boomlink.md's PR 2 scope ("without committing generated
  # target code unnecessarily").
  set(BOOMLINK_PYTHON_PB_DIR ${CMAKE_CURRENT_BINARY_DIR}/python_pb2)
  file(MAKE_DIRECTORY ${BOOMLINK_PYTHON_PB_DIR})
  set(BOOMLINK_PYTHON_PB_FILES "")
  foreach(proto_name IN LISTS BOOMLINK_PROTO_NAMES)
    list(APPEND BOOMLINK_PYTHON_PB_FILES ${BOOMLINK_PYTHON_PB_DIR}/${proto_name}_pb2.py)
  endforeach()
  add_custom_command(
    OUTPUT ${BOOMLINK_PYTHON_PB_FILES}
    COMMAND ${Python3_EXECUTABLE} -m grpc_tools.protoc
            -I${BOOMLINK_PROTO_DIR}
            --python_out=${BOOMLINK_PYTHON_PB_DIR}
            ${BOOMLINK_PROTO_FILES}
    DEPENDS ${BOOMLINK_PROTO_FILES}
    COMMENT "Generating host Python Protobuf classes"
    VERBATIM
  )
  add_custom_target(boomlink_python_pb2 ALL DEPENDS ${BOOMLINK_PYTHON_PB_FILES})

  add_test(NAME boomlink_python_tests
    COMMAND ${Python3_EXECUTABLE} -m pytest ${CMAKE_CURRENT_SOURCE_DIR}/tests -v
  )
  set_tests_properties(boomlink_python_tests PROPERTIES
    ENVIRONMENT "BOOMLINK_CODEC_TOOL=$<TARGET_FILE:boomlink_codec_tool>;PYTHONPATH=${BOOMLINK_PYTHON_PB_DIR}"
  )
endif()

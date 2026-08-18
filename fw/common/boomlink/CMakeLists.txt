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

# Both options are declared only in the standalone branch, not
# unconditionally: neither is meaningful under the ARM cross build (no
# sanitizer runtime exists there, and a firmware build should not be gated on
# a host-toolchain warning becoming an error), and declaring them anyway
# would put two settable-but-inert knobs in the firmware build's `cmake -LH`
# output for a reader to wonder about.
if(BOOMLINK_STANDALONE)
  # ASan+UBSan for the host test suite - this package's whole reason to exist
  # as a host-testable build is exercising a wire-format parser on
  # hostile/malformed bytes (see tests/test_compatibility.py); a memory-safety
  # bug in that parser (or in the C tool driving it) is exactly what a
  # sanitizer catches and a plain pass/fail exit code does not. Applied at
  # directory scope so it reaches the vendored nanopb runtime too -
  # instrumenting pb_decode.c is precisely where you want a sanitizer on a
  # hostile-input parser (deliberately unlike the warning flags below, which
  # are target-scoped to keep third-party code out).
  option(BOOMLINK_SANITIZE "Build with AddressSanitizer + UndefinedBehaviorSanitizer" OFF)
  if(BOOMLINK_SANITIZE)
    add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=all -g -O1)
    add_link_options(-fsanitize=address,undefined)
  endif()

  # Without this, -Wall -Wextra (set on this package's own targets further
  # down) are advisory only: nothing in this repo sets -Werror and neither CI
  # workflow registers a compiler problem matcher, so a new warning would
  # neither fail the build nor surface as a PR annotation - it would just sit
  # in raw log output nobody opens, on a package whose test suite exists
  # specifically to catch C-safety bugs. Defaults OFF so a developer's local
  # build is never blocked by a warning from a newer compiler than CI's; the
  # Debug preset and CI turn it ON (see CMakePresets.json).
  option(BOOMLINK_WERROR "Treat warnings as errors in this package's own targets" OFF)
endif()

# Applied via target_compile_options on this package's own targets only (see
# boomlink_protocol/boomlink_codec_tool below), never add_compile_options:
# the vendored nanopb runtime is third-party code this package doesn't
# control the warning-cleanliness of.
set(BOOMLINK_WARNING_FLAGS -Wall -Wextra)
if(BOOMLINK_WERROR)
  list(APPEND BOOMLINK_WARNING_FLAGS -Werror)
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
# Collapse the ../../ the default above is built from, so every compile line
# and compile_commands.json entry carries a clean -I path instead of
# ".../fw/common/boomlink/../../bom-stm32node/third_party/nanopb". Applied to
# whatever value is in effect, including one a consumer set themselves.
cmake_path(NORMAL_PATH BOOMLINK_NANOPB_DIR)
set(BOOMLINK_NANOPB_GENERATOR ${BOOMLINK_NANOPB_DIR}/generator/nanopb_generator.py)

if(NOT EXISTS ${BOOMLINK_NANOPB_GENERATOR})
  message(FATAL_ERROR
    "Nanopb submodule not found at ${BOOMLINK_NANOPB_DIR}. Run: "
    "git submodule update --init fw/bom-stm32node/third_party/nanopb")
endif()

# The generator is a Python PACKAGE, not just the one entry-point script, so
# depending on nanopb_generator.py alone would miss a submodule bump that
# changes generated output via one of these instead - the generated .pb.h
# bounds would silently stay stale on an incremental build after
# `git submodule update`. Deliberately does NOT list generator/proto/
# nanopb_pb2.py: that file is itself produced by the generator at build time,
# so depending on it would mean depending on a build artifact.
set(BOOMLINK_NANOPB_GENERATOR_FILES
  ${BOOMLINK_NANOPB_GENERATOR}
  ${BOOMLINK_NANOPB_DIR}/generator/proto/__init__.py
  ${BOOMLINK_NANOPB_DIR}/generator/proto/_utils.py
  ${BOOMLINK_NANOPB_DIR}/generator/proto/nanopb.proto
)

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
  # Absolute paths, not "requirements.txt"/"in fw/common/boomlink": this file
  # is add_subdirectory()'d from fw/bom-stm32node, so the firmware build hits
  # this error with a completely different working directory - where a bare
  # `pip install -r requirements.txt` refers to a file that isn't there.
  message(FATAL_ERROR
    "Python packages 'protobuf' and 'grpcio-tools' are required to generate "
    "Nanopb code from ${CMAKE_CURRENT_SOURCE_DIR}/proto/*.proto, using the "
    "interpreter CMake found (${Python3_EXECUTABLE}). Install them with:\n"
    "    ${Python3_EXECUTABLE} -m pip install -r ${CMAKE_CURRENT_SOURCE_DIR}/requirements.txt\n"
    "or run `task setup` in ${CMAKE_CURRENT_SOURCE_DIR} and build via `task "
    "build`, which puts that venv on PATH so this check finds it.")
endif()

set(BOOMLINK_PROTO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/proto)
set(BOOMLINK_NANOPB_OPTIONS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/nanopb)
set(BOOMLINK_PROTO_NAMES common header system envelope)
list(TRANSFORM BOOMLINK_PROTO_NAMES PREPEND ${BOOMLINK_PROTO_DIR}/ OUTPUT_VARIABLE BOOMLINK_PROTO_FILES)
list(TRANSFORM BOOMLINK_PROTO_FILES APPEND .proto)

# Every *.options file in nanopb/ (today, just system.options - see below),
# not hardcoded to that one name, so adding one for a future .proto per this
# file's own pattern (and README.md's "Adding a new message group"
# instructions) needs no edit here.
file(GLOB BOOMLINK_NANOPB_OPTIONS_FILES CONFIGURE_DEPENDS ${BOOMLINK_NANOPB_OPTIONS_DIR}/*.options)

# CONFIGURE_DEPENDS above only guarantees a RECONFIGURE when the glob's
# result set changes. Whether the generator actually re-runs after that is
# decided purely by comparing mtimes of the surviving DEPENDS entries against
# the generated outputs - so any set change whose remaining members are all
# older than those outputs is a silent no-op, and the stale generated bounds
# survive. That is not a corner case: it swallows a DELETED options file, a
# RENAMED one (rename(2) preserves mtime, so the "add" half can't rescue it),
# and even an added file carrying an old mtime (cp -p, tar -p, rsync -t, or
# moved in from elsewhere). All three verified directly against this build.
#
# Stamping the glob's result set into a file turns that set into a real build
# dependency: file(CONFIGURE) rewrites - and therefore re-timestamps - the
# stamp only when the content differs, so a changed set forces regeneration
# while an unchanged one causes no spurious rebuild. Verified all four
# behaviours (delete/rename/old-mtime add regenerate; no-op rebuild stays a
# no-op).
set(BOOMLINK_NANOPB_OPTIONS_STAMP ${CMAKE_CURRENT_BINARY_DIR}/nanopb_options_set.stamp)
file(CONFIGURE OUTPUT ${BOOMLINK_NANOPB_OPTIONS_STAMP}
     CONTENT "${BOOMLINK_NANOPB_OPTIONS_FILES}\n")

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
    DEPENDS ${BOOMLINK_PROTO_FILES}
            ${BOOMLINK_NANOPB_OPTIONS_FILES}
            ${BOOMLINK_NANOPB_OPTIONS_STAMP}
            ${BOOMLINK_NANOPB_GENERATOR_FILES}
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
# Deliberately OUTSIDE the standalone branch below, so the ARM cross build
# gets these too. This target is this package's own code in both modes (the
# vendored-third-party argument that keeps these flags off boomlink_nanopb
# does not apply to it), and the firmware is the build that needs them more:
# the host build has ASan/UBSan as a runtime net, while the ARM build has no
# sanitizer runtime at all and otherwise sees only -Wall (no -Wextra) from
# fw/bom-stm32node/cmake/gcc-arm-none-eabi.cmake. Verified zero warnings from
# boomlink_codec.c and all four generated .pb.c under both compilers.
target_compile_options(boomlink_protocol PRIVATE ${BOOMLINK_WARNING_FLAGS})

if(BOOMLINK_STANDALONE)
  enable_testing()

  # The only target that compiles the hand-rolled buffer/argument-parsing
  # work in tests/codec_tool.c, and host-only by nature - a cross-compiled
  # host tool makes no sense.
  add_executable(boomlink_codec_tool tests/codec_tool.c)
  target_link_libraries(boomlink_codec_tool PRIVATE boomlink_protocol)
  target_compile_options(boomlink_codec_tool PRIVATE ${BOOMLINK_WARNING_FLAGS})

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

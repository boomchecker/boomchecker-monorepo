# 3.21, not 3.20: CMakePresets.json's testPresets (used by `ctest --preset`)
# need 3.21 - a lower cmake_minimum_required here would understate what this
# file actually needs to be usable end-to-end.
cmake_minimum_required(VERSION 3.21)

# This file is consumed two different ways:
#
# 1. add_subdirectory()'d from fw/bom-stm32node/CMakeLists.txt, under that
#    project's ARM cross toolchain - produces the `boomlink_protocol` static
#    library (generated Nanopb codec + boomlink_codec.c), `boomlink_linkframe`
#    (the link frame header) and `boomlink_linkengine` (the link engine), all
#    three now actually linked into the firmware image (`App/link/` is the
#    link engine's call site - see its own comment below for how that came
#    to be true only as of Phase C). No tests are built this way:
#    cross-compiled host tooling makes no sense, and the STM32 target has no
#    way to run pytest.
# 2. configured standalone, with the host's native compiler - additionally
#    builds `boomlink_codec_tool` and `boomlink_linkframe_tool` (small CLIs
#    used by the Python tests) and registers the native/host test suite via
#    CTest. This is what `cmake --preset Debug` in this directory does (see
#    CMakePresets.json).
#
# boomlink.md section 4 requires the .proto files to not live only inside the
# STM32 target; this dual-mode file is how that requirement is met without
# maintaining the Nanopb-generation logic twice.
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  # CXX is enabled for one reason and builds nothing: it makes
  # CMAKE_CXX_COMPILER available to the boomlink_linkframe_cxx_bound test below,
  # which compiles two throwaway C++ translation units to check that
  # boomlink_linkframe.h's encoder bound still holds for C++ callers. Every
  # library and tool here is C. Under the firmware's ARM build this branch does
  # not run, and that project enables CXX for its own reasons anyway.
  project(boomlink_protocol LANGUAGES C CXX)
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
  # Opt-in enforcement for a pipeline that wants to guarantee the suite is
  # actually instrumented rather than trust a preset was used.
  option(BOOMLINK_REQUIRE_SANITIZERS
         "Fail configuration if BOOMLINK_SANITIZE is OFF" OFF)
  if(BOOMLINK_SANITIZE)
    add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=all -g -O1)
    add_link_options(-fsanitize=address,undefined)
  elseif(BOOMLINK_REQUIRE_SANITIZERS)
    message(FATAL_ERROR
      "BOOMLINK_REQUIRE_SANITIZERS is ON but BOOMLINK_SANITIZE is OFF. Configure with "
      "`cmake --preset Debug`, or pass -DBOOMLINK_SANITIZE=ON.")
  else()
    # Warn at CONFIGURE time, not only from the test suite's own output. The
    # suite does report it (tests/conftest.py's pytest_report_header), but
    # pytest hides its header under -q and ctest hides all output on a passing
    # test - so on the all-green run, which is exactly the run that gives false
    # confidence, that line is never seen. This one survives, because CMake
    # warnings are printed on every configure and kept in CI logs.
    message(WARNING
      "BOOMLINK_SANITIZE is OFF: this package's negative-path tests feed deliberately "
      "malformed bytes to a parser, and without ASan/UBSan they cannot tell an "
      "intentional rejection from a memory-safety bug - a real one passes green. "
      "Configure with `cmake --preset Debug` (which enables it) unless you specifically "
      "want the faster uninstrumented build.")
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

# Applied via target_compile_options on this package's own targets only -
# boomlink_protocol, boomlink_linkframe and both test tools below - never
# add_compile_options: the vendored nanopb runtime is third-party code this
# package doesn't control the warning-cleanliness of.
set(BOOMLINK_WARNING_FLAGS -Wall -Wextra)

# -Werror is kept SEPARATE from the warning flags above, and is applied only
# to the hand-written translation units - boomlink_codec.c (via a source
# property, since it shares boomlink_protocol with generated code) plus
# boomlink_linkframe.c and both tools' sources, whose targets contain nothing
# generated and so take it target-wide - never to the four generated *.pb.c.
# Same reasoning that keeps every warning
# flag off boomlink_nanopb: those files are machine output from the vendored
# generator, so a submodule bump or a newer compiler on CI's runner image
# could hard-fail the build over code nobody in this repo wrote or can fix.
# -Wall -Wextra still cover them, so a warning there is still visible - it
# just isn't fatal.
#
# Guarded on BOOMLINK_STANDALONE, not only on the option's value: the option
# is declared only in that branch, so under the ARM cross build the bare
# `if(BOOMLINK_WERROR)` would silently read an uninitialized cache entry and
# a -D on the firmware build's command line would take effect with nothing in
# `cmake -LH` to discover it from - the opposite of how BOOMLINK_SANITIZE
# behaves there (inert, and CMake warns it was unused). Keeping both knobs
# inert in the firmware build makes them predictable; firmware warning policy
# belongs in fw/bom-stm32node/cmake/gcc-arm-none-eabi.cmake.
set(BOOMLINK_WERROR_FLAGS "")
if(BOOMLINK_STANDALONE AND BOOMLINK_WERROR)
  set(BOOMLINK_WERROR_FLAGS -Werror)
endif()

# One exception to all of the above, and the only warning knob declared in BOTH
# modes: -Werror for this package's TARGET-BOUND libraries - boomlink_linkframe
# and boomlink_linkengine - off by default.
#
# It exists because three places (this file, README.md and
# .github/workflows/test-bom-stm32node-build.yaml) justify cross-compiling those
# libraries as proving they are "free of host-isms AND warning-clean for the
# target". Only the first half was true: nothing put -Werror on the ARM build,
# and the workflow's check only asserts the archive exists - so adding
# `static int unused_var = 7;` to boomlink_linkframe.c left the ARM build green
# with the warning buried in a log (verified), while the same edit is a hard
# error in the host build. That gap matters more than it sounds, because the
# interesting warnings here are precisely the ones the host build CANNOT see:
# int/long/size_t are different widths on the target, so -Wformat and
# sign-compare diagnostics appear there and nowhere else.
#
# Named for what it covers rather than BOOMLINK_LINKFRAME_WERROR, which is what
# it was called when the frame layer was the only such target. It now gates two,
# and a knob whose name says "linkframe" while silently also governing the engine
# is the kind of thing that gets switched off for a reason applying to only one of
# them.
#
# Default OFF, ON in CI, for exactly the reason BOOMLINK_WERROR is: a developer's
# local firmware build must never be blocked by a warning from a compiler newer
# than CI's, least of all on code they did not touch. Scoped to these two
# hand-written libraries, so it cannot spread to the CubeMX-generated sources or
# the vendored libraries.
option(BOOMLINK_TARGET_WERROR
       "Treat warnings as errors in the libraries cross-compiled for the target - \
boomlink_linkframe, boomlink_linkengine, boomlink_dispatch, boomlink_command_service \
and boomlink_config_service (CI turns this on)" OFF)
set(BOOMLINK_TARGET_WERROR_FLAGS "")
if(BOOMLINK_TARGET_WERROR)
  set(BOOMLINK_TARGET_WERROR_FLAGS -Werror)
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
  # find_package(Python3) caches the interpreter it found, so without this the
  # remedy below could not work: `task setup` creates the venv and `task build`
  # puts it first on PATH, but a re-configure in the same build directory would
  # keep using the CACHED bare python3 that failed, print no "Found Python3"
  # line at all, and fail identically with no clue why - a closed loop on the
  # primary onboarding path, only escapable via `task clean`. Verified: dropping
  # these entries makes the very next configure pick up the venv. Cleared
  # BEFORE the error so the state is already fixed when the developer retries.
  unset(Python3_EXECUTABLE CACHE)
  unset(_Python3_EXECUTABLE CACHE)

  # Absolute paths, not "requirements.txt"/"in fw/common/boomlink": this file
  # is add_subdirectory()'d from fw/bom-stm32node, so the firmware build hits
  # this error with a completely different working directory - where a bare
  # `pip install -r requirements.txt` refers to a file that isn't there.
  #
  # The `task build` hint has to differ per mode: `task build` in THIS
  # package's directory builds the host library, so pointing a firmware
  # developer at it would leave them exactly where they started. Both branches
  # must name `task build` though (not just "build again") - `task setup` only
  # CREATES .venv; nothing puts it on PATH except task build/task test.
  #
  # Single quoted strings: `set(VAR "a" "b")` makes a two-element LIST, and the
  # ";" separator then leaks into the rendered message.
  if(BOOMLINK_STANDALONE)
    set(BOOMLINK_DEPS_BUILD_HINT
        "then build via `task build` in ${CMAKE_CURRENT_SOURCE_DIR}, which puts that venv on PATH so this check finds it")
  else()
    set(BOOMLINK_DEPS_BUILD_HINT
        "then build via `task build` in ${CMAKE_SOURCE_DIR} (the firmware directory - `task build` in the path above builds only the host library), which puts that venv on PATH so this check finds it")
  endif()
  message(FATAL_ERROR
    "Python packages 'protobuf' and 'grpcio-tools' are required to generate "
    "Nanopb code from ${CMAKE_CURRENT_SOURCE_DIR}/proto/*.proto, using the "
    "interpreter CMake found (${Python3_EXECUTABLE}). Install them with:\n"
    "    ${Python3_EXECUTABLE} -m pip install -r ${CMAKE_CURRENT_SOURCE_DIR}/requirements.txt\n"
    "or run `task setup` in ${CMAKE_CURRENT_SOURCE_DIR}, ${BOOMLINK_DEPS_BUILD_HINT}.")
endif()

set(BOOMLINK_PROTO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/proto)
set(BOOMLINK_NANOPB_OPTIONS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/nanopb)
set(BOOMLINK_PROTO_NAMES common header system detection telemetry command config envelope)
list(TRANSFORM BOOMLINK_PROTO_NAMES PREPEND ${BOOMLINK_PROTO_DIR}/ OUTPUT_VARIABLE BOOMLINK_PROTO_FILES)
list(TRANSFORM BOOMLINK_PROTO_FILES APPEND .proto)

# Every *.options file in both directories a CHECKED-IN one can live in, not
# hardcoded to a name, so adding one for a future .proto per this file's own
# pattern (and README.md's "Adding a new message group" instructions) needs no
# edit here.
#
# nanopb/ is this package's convention and where README.md tells contributors
# to put them. proto/ is globbed too because the generator finds one there as
# well - it searches every -I path (see the add_custom_command below, which
# passes -I${BOOMLINK_PROTO_DIR}), and an options file sitting next to its
# .proto is nanopb's OWN upstream convention, so it is the arrangement someone
# following nanopb's documentation rather than this package's would reach for.
# Globbing only nanopb/ left that location honored-but-completely-untracked:
# verified an edit to proto/system.options was applied on a clean build and
# then silently ignored FOREVER on every incremental build afterwards, even
# with an mtime newer than the generated outputs - strictly worse than the
# mtime-window gap described below, since no amount of touching the file helps.
#
# There is a THIRD location, deliberately not globbed: the generator searches
# its own working directory (".") first of all, which for the custom command
# below is CMAKE_CURRENT_BINARY_DIR. Nothing checked in can live there and the
# build tree is gitignored, so tracking it would be meaningless - but it does
# mean a stray options file in the build directory silently outranks both
# directories globbed here.
file(GLOB BOOMLINK_NANOPB_OPTIONS_FILES CONFIGURE_DEPENDS
     ${BOOMLINK_NANOPB_OPTIONS_DIR}/*.options
     ${BOOMLINK_PROTO_DIR}/*.options)

# Blessing both locations (above) makes a same-basename collision reachable,
# and the generator resolves one silently: it takes the FIRST -I path that has
# a matching <proto>.options and stops - no merge, no warning, and the
# "Reading options from ..." trace only appears under -v, which the custom
# command does not pass. Verified nanopb/system.options (max_size:200) being
# silently beaten by proto/system.options (max_size:48), with the loser - which
# is where this package keeps the derivation of its radio-budget ceiling -
# becoming dead weight with no signal at all. The wrong bound then surfaces
# only indirectly and confusingly (a golden vector failing to decode, or the
# budget _Static_assert in boomlink_codec.c), never as "you have two of these".
set(BOOMLINK_SEEN_OPTIONS_BASENAMES "")
foreach(options_file IN LISTS BOOMLINK_NANOPB_OPTIONS_FILES)
  get_filename_component(options_basename ${options_file} NAME)
  if(options_basename IN_LIST BOOMLINK_SEEN_OPTIONS_BASENAMES)
    message(FATAL_ERROR
      "Two Nanopb options files are both named '${options_basename}':\n"
      "    ${BOOMLINK_NANOPB_OPTIONS_DIR}/${options_basename}\n"
      "    ${BOOMLINK_PROTO_DIR}/${options_basename}\n"
      "The generator would silently use whichever -I path comes first and ignore "
      "the other, so the bounds actually compiled in would not be the ones you "
      "think. Keep exactly one - nanopb/ is this package's convention (see "
      "README.md).")
  endif()
  list(APPEND BOOMLINK_SEEN_OPTIONS_BASENAMES ${options_basename})
endforeach()

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
# no-op), on both the Ninja and Makefile generators.
#
# Note the stamp tracks the SET of options files; their CONTENTS are tracked
# the ordinary way, by listing the files themselves in DEPENDS below.
set(BOOMLINK_NANOPB_OPTIONS_STAMP ${CMAKE_CURRENT_BINARY_DIR}/nanopb_options_set.stamp)
file(CONFIGURE OUTPUT ${BOOMLINK_NANOPB_OPTIONS_STAMP}
     CONTENT "${BOOMLINK_NANOPB_OPTIONS_FILES}\n")

set(BOOMLINK_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${BOOMLINK_GENERATED_DIR})

set(BOOMLINK_GENERATED_SOURCES "")
foreach(proto_name IN LISTS BOOMLINK_PROTO_NAMES)
  set(out_c ${BOOMLINK_GENERATED_DIR}/${proto_name}.pb.c)
  set(out_h ${BOOMLINK_GENERATED_DIR}/${proto_name}.pb.h)
  # Every generated file depends on all four .proto sources and every tracked
  # *.options file from both globbed directories (not just its own), rather
  # than tracking
  # envelope.proto's imports or each proto's own single options file
  # precisely: simpler, and the cost of an occasional unnecessary
  # regeneration of these four tiny files is not worth the extra
  # bookkeeping.
  #
  # No explicit -f<options file>: passing one global options file to every
  # invocation made the generator warn "did not match any fields" on every
  # proto file with no matching options entry, every single build - noise
  # that buries the one thing that warning is supposed to catch (a genuinely
  # stale/typo'd entry). Nanopb auto-discovers a same-named `<proto>.options`
  # via its `-I`/`--options-path` search list instead (verified against the
  # vendored generator source): only system.proto and command.proto have one
  # (nanopb/*.options, matched via the extra -I below), so the rest simply
  # find nothing and stay silent.
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
# Note the second entry is this package's ROOT, not a narrow include directory:
# boomlink_codec.h sits there, so that is what has to be exported for the
# firmware to include it. The side effect is that everything else directly under
# fw/common/boomlink becomes includable by every consumer of boomlink_protocol -
# which today means <linkframe/boomlink_linkframe.h> is reachable from firmware
# code that links only the codec. That is one-directional and harmless in this
# direction (the link frame header is standalone, and its own target is what
# provides the implementation), but it is NOT the compile-time boundary the
# linkframe target's comment below describes: that boundary stops the codec's
# headers reaching link-layer code, not the other way round. Worth keeping in
# mind before adding anything private to this directory.
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
# -Werror on the hand-written TU only, not on this target's four generated
# *.pb.c - see BOOMLINK_WERROR_FLAGS above for why. A source-file property
# rather than a second target, since the generated and hand-written sources
# share one library.
if(BOOMLINK_WERROR_FLAGS)
  # APPEND, not PROPERTIES: a plain set replaces COMPILE_OPTIONS outright, so a
  # second such call (here or in a consumer) would silently clobber one of the
  # two, order-dependently. Nothing else sets it today - this is to keep that
  # true by construction rather than by luck.
  set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/boomlink_codec.c
    APPEND PROPERTY COMPILE_OPTIONS ${BOOMLINK_WERROR_FLAGS})
endif()

# BoomLink's link frame header (boomlink.md section 7.3), a SEPARATE target
# from boomlink_protocol on purpose - and the separation is the point, not
# tidiness. Section 9 requires that "BoomLink never decodes the Protobuf
# payload and has no Nanopb dependency": the link layer must be able to filter,
# acknowledge and deduplicate a packet, and reject foreign traffic from a few
# leading bytes, without invoking a Protobuf decoder. Because this target does
# not link boomlink::nanopb and does not include the codec's directory, that
# boundary is enforced by the compiler - unlike boomlink_protocol's own
# "consumers shouldn't call pb_encode directly" convention, which nothing
# stops (see its comment above). Do not add a nanopb link here to make
# something compile; if link-layer code needs to look inside the payload, the
# layering is wrong.
#
# Built in the ARM cross build, and - as of Phase C - actually linked into
# the firmware too (fw/bom-stm32node/App/link/ is the call site; see that
# target's own comment below for the fuller history of what this looked like
# before Phase C landed). Cross-compiling it here, rather than only once
# something depended on it, is what proves it free of host-isms and
# warning-clean for the target ahead of time - the second half of that claim
# is only true when BOOMLINK_TARGET_WERROR is on, which is why CI turns it on
# - see that option above.
#
# Precisely, before Phase C: the archive was BUILT but never reached the
# linker at all - not on bom-stm32node.elf's link line, because nothing linked
# it, which is a stronger statement than "the linker just drops it for now"
# (that would imply the archive was already on the link line and only a
# reference was missing). Phase C's fw/bom-stm32node/CMakeLists.txt now links
# boomlink_linkengine, which PUBLIC-links this target too - see that file.
add_library(boomlink_linkframe STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/linkframe/boomlink_linkframe.c
)
target_include_directories(boomlink_linkframe PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/linkframe
)
# _Static_assert (C11), same self-contained reasoning as boomlink_protocol.
target_compile_features(boomlink_linkframe PUBLIC c_std_11)
# BOOMLINK_WERROR_FLAGS is the host-build knob (empty outside the standalone
# branch); BOOMLINK_TARGET_WERROR_FLAGS is the one that also applies under the
# ARM cross build, which is what makes "warning-clean for the target" a checked
# claim rather than a stated one. Both are listed so either can enable it.
target_compile_options(boomlink_linkframe PRIVATE
  ${BOOMLINK_WARNING_FLAGS} ${BOOMLINK_WERROR_FLAGS} ${BOOMLINK_TARGET_WERROR_FLAGS})

# BoomLink's link engine (boomlink.md section 9): the stateful half of the link
# layer - addressing, ACK/retry, duplicate suppression, TX queue - as opposed to
# boomlink_linkframe's stateless header codec.
#
# A SEPARATE target from boomlink_linkframe, and the separation is the same one
# section 9 draws: this depends on the frame layer, never the reverse. Section 9's
# "BoomLink never decodes the Protobuf payload and has no Nanopb dependency"
# applies to the engine at least as much as to the header codec - the engine is
# what would be tempted to look inside a payload to make a routing or retry
# decision.
#
# That property is NOT inherited from the frame layer, and an earlier version of
# this comment claimed it was ("it links only boomlink_linkframe, so the check
# registered below covers it too"). Linking a Nanopb-free library says nothing
# about what this target's OWN four sources include: a relative include reaching
# nanopb's headers needs no CMake change at all, and emits no symbol at all if it
# only uses a macro or a typedef. Verified - such an include in boomlink_link.c
# left every one of the twelve tests green. All four translation units are
# therefore named explicitly in the check registered further down, alongside this
# archive.
#
# Built in the ARM cross build for the same reason boomlink_linkframe is: to
# prove it is free of host-isms and (with BOOMLINK_TARGET_WERROR) warning-clean
# for the target. Phase C (fw/bom-stm32node/App/link/) now links it - the
# workflow's "archive is produced" assertion below predates that and stays
# useful anyway: it is what caught this archive never being cross-compiled at
# all, when this target was briefly marked EXCLUDE_FROM_ALL, at a point in
# review where nothing yet linked it and nothing else would have noticed.
add_library(boomlink_linkengine STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/linkengine/boomlink_port.c
  ${CMAKE_CURRENT_SOURCE_DIR}/linkengine/boomlink_dupcache.c
  ${CMAKE_CURRENT_SOURCE_DIR}/linkengine/boomlink_txqueue.c
  ${CMAKE_CURRENT_SOURCE_DIR}/linkengine/boomlink_link.c
)
target_include_directories(boomlink_linkengine PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/linkengine
)
target_link_libraries(boomlink_linkengine PUBLIC boomlink_linkframe)
target_compile_features(boomlink_linkengine PUBLIC c_std_11)
target_compile_options(boomlink_linkengine PRIVATE
  ${BOOMLINK_WARNING_FLAGS} ${BOOMLINK_WERROR_FLAGS} ${BOOMLINK_TARGET_WERROR_FLAGS})

# PR 4's protocol dispatcher (boomlink.md section 4's "what actually landed"
# for the link engine explains the precedent this follows): target-agnostic
# C that routes a decoded Envelope by which_payload, deliberately outside
# fw/bom-stm32node/App/protocol/ despite that being section 4's original
# sketch location, so it is host-testable the same way the link engine is.
#
# Depends on boomlink_protocol (PUBLIC: this module's own header includes
# envelope.pb.h), the opposite dependency direction from boomlink_linkframe/
# boomlink_linkengine, which must NOT depend on Nanopb (section 9) - this
# module's entire job is looking inside what those two deliberately never
# do, so the same rule points the other way here.
add_library(boomlink_dispatch STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/dispatch/boomlink_dispatch.c
  ${CMAKE_CURRENT_SOURCE_DIR}/dispatch/boomlink_envelope_builder.c
)
target_include_directories(boomlink_dispatch PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/dispatch
)
target_link_libraries(boomlink_dispatch PUBLIC boomlink_protocol)
target_compile_features(boomlink_dispatch PUBLIC c_std_11)
target_compile_options(boomlink_dispatch PRIVATE
  ${BOOMLINK_WARNING_FLAGS} ${BOOMLINK_WERROR_FLAGS} ${BOOMLINK_TARGET_WERROR_FLAGS})

# Section 8.3's command service (boomlink.md), against the dispatcher's
# boomlink_dispatch_command_fn seam. Actions are injected (boomlink_command_
# service.h's boomlink_command_ops_t) rather than performed here, so this
# stays host-testable against a fake ops struct - Phase C wires the real
# ones (NVIC reset, an LED, ...).
add_library(boomlink_command_service STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/services/boomlink_command_service.c
)
target_include_directories(boomlink_command_service PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/services
)
target_link_libraries(boomlink_command_service PUBLIC boomlink_dispatch)
target_compile_features(boomlink_command_service PUBLIC c_std_11)
target_compile_options(boomlink_command_service PRIVATE
  ${BOOMLINK_WARNING_FLAGS} ${BOOMLINK_WERROR_FLAGS} ${BOOMLINK_TARGET_WERROR_FLAGS})

# Section 8.2's ConfigGet/ConfigSet over an in-memory NodeConfig, including
# the revert-on-timeout apply that section requires for RadioConfig/magic/
# node_id changes. Persistence to flash (section 10.1) is Phase B's job.
add_library(boomlink_config_service STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/services/boomlink_config_service.c
)
target_include_directories(boomlink_config_service PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/services
)
# boomlink_linkframe is PRIVATE: only boomlink_config_service.c itself uses
# it, for BOOMLINK_ADDR_INVALID/BOOMLINK_ADDR_BROADCAST (section 7.2's
# reserved node_id values) - reused rather than duplicated as bare hex
# literals. Header-only use (both constants are #define, not a linked
# symbol), but declared as a real dependency rather than an extra include
# path so it shows up in the link graph like any other real one.
target_link_libraries(boomlink_config_service PUBLIC boomlink_dispatch PRIVATE boomlink_linkframe)
target_compile_features(boomlink_config_service PUBLIC c_std_11)
target_compile_options(boomlink_config_service PRIVATE
  ${BOOMLINK_WARNING_FLAGS} ${BOOMLINK_WERROR_FLAGS} ${BOOMLINK_TARGET_WERROR_FLAGS})

if(BOOMLINK_STANDALONE)
  enable_testing()

  # Argument/hex parsing shared by both CLI test tools. Compiled into each
  # rather than made its own library: two small executables, and a library
  # would add a target for ~90 lines. See tests/tool_support.h for why it is
  # shared at all instead of copied - parse_u32 has a history of subtle bugs
  # that a second hand-maintained copy would eventually reintroduce.
  #
  # One consequence of listing the source in two targets: it is compiled twice,
  # with each target's own include paths and definitions. That is wanted here -
  # BOOMLINK_SANITIZE_ENABLED is per-target, and tool_support.h's
  # instrumentation cross-check has to see each tool's value - but it also means
  # tool_support.c must not depend on anything only one of the two tools puts on
  # the include path. It includes nothing but its own header and libc, which is
  # what keeps that true; a shared library target would enforce it instead.
  set(BOOMLINK_TOOL_SUPPORT ${CMAKE_CURRENT_SOURCE_DIR}/tests/tool_support.c)

  # The only target that compiles the hand-rolled buffer/argument-parsing
  # work in tests/codec_tool.c, and host-only by nature - a cross-compiled
  # host tool makes no sense.
  add_executable(boomlink_codec_tool tests/codec_tool.c ${BOOMLINK_TOOL_SUPPORT})
  # boomlink_linkframe is linked for its HEADER only: codec_tool carries the
  # _Static_assert pinning BOOMLINK_LINKFRAME_HEADER_SIZE (the link layer's
  # own definition) to BOOMLINK_LINK_FRAME_HEADER_SIZE (the codec's copy, used
  # to compute the on-air Envelope budget). Neither layer may include the
  # other's header - that is the boundary above - so a test tool that
  # legitimately sees both is the only place the two numbers can be checked
  # against each other instead of silently drifting apart.
  target_link_libraries(boomlink_codec_tool PRIVATE boomlink_protocol boomlink_linkframe)
  # Hand-written, so it gets -Werror too when enabled (unlike the generated
  # *.pb.c - see BOOMLINK_WERROR_FLAGS).
  target_compile_options(boomlink_codec_tool PRIVATE
    ${BOOMLINK_WARNING_FLAGS} ${BOOMLINK_WERROR_FLAGS})
  # Lets `codec_tool limits` report whether it was built instrumented, so the
  # pytest suite can say so instead of silently losing its safety net: with
  # BOOMLINK_SANITIZE=OFF, assert_clean_rejection() degrades to little more
  # than "exit code was nonzero" and a genuine memory-safety bug on a
  # rejection path passes green (verified). A compile definition is needed
  # because there is no portable way to detect this from inside the code:
  # GCC defines __SANITIZE_ADDRESS__ for ASan but has no UBSan equivalent.
  target_compile_definitions(boomlink_codec_tool PRIVATE
    BOOMLINK_SANITIZE_ENABLED=$<IF:$<BOOL:${BOOMLINK_SANITIZE}>,1,0>)

  add_test(NAME boomlink_codec_selftest COMMAND boomlink_codec_tool selftest)

  # The link frame's own CLI, so the pytest suite can cross-check the C
  # encoder/parser against linkframe/boomlink_linkframe.py on the same wire
  # bytes. Deliberately does NOT link boomlink_protocol: this tool exists to
  # exercise a layer that must work without a Protobuf decoder, so keeping one
  # off its link line keeps the tool honest about what it is testing.
  #
  # Note this is a convention, not something the symbol check can police:
  # linking boomlink_protocol here would leave the tool's binary with zero pb_
  # symbols, because nothing in linkframe_tool.c references the codec and the
  # linker pulls no members out of an archive it does not need. Verified. The
  # check only fires once the tool actually calls into the codec.
  add_executable(boomlink_linkframe_tool tests/linkframe_tool.c ${BOOMLINK_TOOL_SUPPORT})
  target_link_libraries(boomlink_linkframe_tool PRIVATE boomlink_linkframe)
  target_compile_options(boomlink_linkframe_tool PRIVATE
    ${BOOMLINK_WARNING_FLAGS} ${BOOMLINK_WERROR_FLAGS})
  target_compile_definitions(boomlink_linkframe_tool PRIVATE
    BOOMLINK_SANITIZE_ENABLED=$<IF:$<BOOL:${BOOMLINK_SANITIZE}>,1,0>)

  add_test(NAME boomlink_linkframe_selftest COMMAND boomlink_linkframe_tool selftest)

  # The link engine's tests are C scenarios, not pytest, and that is a departure
  # from how the frame layer is tested - so the reason matters. The frame layer's
  # subject is a WIRE FORMAT, where an independent Python implementation is worth
  # its weight: it catches a misreading of the spec that a single implementation
  # would happily agree with itself about. The engine's subject is a stateful
  # SEQUENCE (send, no ACK, advance the clock, expect a retry with the same
  # sequence number, advance again, expect final failure). Driving that through a
  # subprocess needs either a scenario language or one subcommand per scenario,
  # and reimplementing a retry state machine in Python would double the work
  # while cross-checking behaviour rather than bytes.
  #
  # What keeps the cross-language check where it earns its keep: the engine's
  # emitted BYTES are still compared against the Python reference, because the
  # frames it produces are frame-layer frames - see the pytest tests that build
  # the expected ACK with boomlink_linkframe.py's make_ack() and compare.
  #
  # fake_port.c is compiled into the test binary rather than shipped in the
  # library: it is a test double, and a target that could link it is a target
  # that could accidentally ship it.
  add_executable(boomlink_port_test tests/port_test.c tests/fake_port.c)
  target_link_libraries(boomlink_port_test PRIVATE boomlink_linkengine)
  target_include_directories(boomlink_port_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  target_compile_options(boomlink_port_test PRIVATE
    ${BOOMLINK_WARNING_FLAGS} ${BOOMLINK_WERROR_FLAGS})
  add_test(NAME boomlink_port_test COMMAND boomlink_port_test)

  # Whether the C harness can FAIL at all. c_test.h's `return 1` is the only path
  # by which any C test here fails, and nothing exercised it - the path only runs
  # when a test is already failing. Changing it to `return 0` made every C binary
  # built from c_test.h exit 0 forever while printing its failures to stderr,
  # demonstrated with two real duplicate-cache bugs green at the same time. One
  # character, and the exact check count it would have hidden only grows as more
  # tests are added - see each binary's own BOOMLINK_TEST_REPORT() floor below for
  # the current number, since a count restated here would drift the moment a test
  # was added and nobody was looking at this comment. The selfcheck binary is
  # built but deliberately NOT registered as a test of its own: it is supposed to
  # fail, and the script is what knows which failures are the expected ones.
  add_executable(boomlink_c_test_selfcheck tests/c_test_selfcheck.c)
  target_include_directories(boomlink_c_test_selfcheck PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  target_compile_options(boomlink_c_test_selfcheck PRIVATE
    ${BOOMLINK_WARNING_FLAGS} ${BOOMLINK_WERROR_FLAGS})
  add_test(NAME boomlink_c_test_harness
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_c_test.sh
            $<TARGET_FILE:boomlink_c_test_selfcheck>)

  # Duplicate suppression (section 9.4) as a pure data structure, so the cases
  # that actually break it can be reached directly: a sequence wrap, both edges
  # of the reordering window, a peer rebooting mid-conversation, and LRU
  # eviction. Driving those through a radio would need thousands of frames and
  # would still not reach the wrap.
  add_executable(boomlink_dupcache_test tests/dupcache_test.c)
  target_link_libraries(boomlink_dupcache_test PRIVATE boomlink_linkengine)
  target_include_directories(boomlink_dupcache_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  target_compile_options(boomlink_dupcache_test PRIVATE
    ${BOOMLINK_WARNING_FLAGS} ${BOOMLINK_WERROR_FLAGS})
  add_test(NAME boomlink_dupcache_test COMMAND boomlink_dupcache_test)

  # The priority TX queue (section 9.8) as a pure data structure. Its two
  # requirements pull against each other - urgent traffic must not wait behind
  # telemetry, and the drop policy must shed telemetry rather than detections -
  # so both directions of the eviction rule need their own cases.
  add_executable(boomlink_txqueue_test tests/txqueue_test.c)
  target_link_libraries(boomlink_txqueue_test PRIVATE boomlink_linkengine)
  target_include_directories(boomlink_txqueue_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  target_compile_options(boomlink_txqueue_test PRIVATE
    ${BOOMLINK_WARNING_FLAGS} ${BOOMLINK_WERROR_FLAGS})
  add_test(NAME boomlink_txqueue_test COMMAND boomlink_txqueue_test)

  # The engine's RX pipeline (section 9.2) and the delivery it exists for. Where
  # delivery is the subject, a scenario runs TWO engines on one fake medium, so a
  # frame is built by one and validated by the other - a test that constructed the
  # expected frame itself would agree with a misreading of section 7.3 as happily
  # as the code does. The rejection scenarios cannot be shaped that way and are
  # forged-and-injected instead: no compliant engine emits a frame from the
  # broadcast address or an ACK addressed to a third party, so a receiver-side
  # guard is only reachable by traffic no sender would produce. An earlier version
  # of this comment claimed EVERY scenario ran two engines.
  add_executable(boomlink_link_rx_test tests/link_rx_test.c tests/fake_port.c)
  target_link_libraries(boomlink_link_rx_test PRIVATE boomlink_linkengine)
  target_include_directories(boomlink_link_rx_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  target_compile_options(boomlink_link_rx_test PRIVATE
    ${BOOMLINK_WARNING_FLAGS} ${BOOMLINK_WERROR_FLAGS})
  add_test(NAME boomlink_link_rx_test COMMAND boomlink_link_rx_test)

  # The TX pipeline (sections 9.1, 9.6, 9.7). A separate binary from the RX one
  # because what it needs from the harness is different in kind: the RX scenarios
  # present traffic and assert on classification, while these advance a clock one
  # millisecond at a time and assert on WHEN the pipeline moves. Section 15.2 lists
  # ACK timeout, retry count and randomized backoff bounds as fake-radio territory
  # precisely because none of them is observable against real hardware.
  add_executable(boomlink_link_tx_test tests/link_tx_test.c tests/fake_port.c)
  target_link_libraries(boomlink_link_tx_test PRIVATE boomlink_linkengine)
  target_include_directories(boomlink_link_tx_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  target_compile_options(boomlink_link_tx_test PRIVATE
    ${BOOMLINK_WARNING_FLAGS} ${BOOMLINK_WERROR_FLAGS})
  add_test(NAME boomlink_link_tx_test COMMAND boomlink_link_tx_test)

  # PR 4 Phase A: the Envelope router (boomlink_dispatch_process()) as a pure
  # function over fake handlers - every which_payload branch, the response
  # header it builds for request/response groups (never the handler's job),
  # and the no_payload/unhandled counters that make a silently dropped
  # message impossible to miss.
  add_executable(boomlink_dispatch_test tests/dispatch_test.c)
  target_link_libraries(boomlink_dispatch_test PRIVATE boomlink_dispatch)
  target_include_directories(boomlink_dispatch_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  target_compile_options(boomlink_dispatch_test PRIVATE
    ${BOOMLINK_WARNING_FLAGS} ${BOOMLINK_WERROR_FLAGS})
  add_test(NAME boomlink_dispatch_test COMMAND boomlink_dispatch_test)

  # PR 4 Phase A: section 8.3's command set against a fake ops struct - every
  # CommandType reaches its own callback (or UNSUPPORTED if that slot is
  # NULL), and a response is always built for a real CommandRequest.
  add_executable(boomlink_command_service_test tests/command_service_test.c)
  target_link_libraries(boomlink_command_service_test PRIVATE boomlink_command_service)
  target_include_directories(boomlink_command_service_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  target_compile_options(boomlink_command_service_test PRIVATE
    ${BOOMLINK_WARNING_FLAGS} ${BOOMLINK_WERROR_FLAGS})
  add_test(NAME boomlink_command_service_test COMMAND boomlink_command_service_test)

  # PR 4 Phase A: section 8.2's ConfigGet/ConfigSet, the whole-group-
  # replacement contract, and the revert-on-timeout apply state machine -
  # the trickiest logic in this phase, and the one most worth pinning a
  # boundary test on (the window closing at exactly confirm_window_ms).
  add_executable(boomlink_config_service_test tests/config_service_test.c)
  target_link_libraries(boomlink_config_service_test PRIVATE boomlink_config_service)
  target_include_directories(boomlink_config_service_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  target_compile_options(boomlink_config_service_test PRIVATE
    ${BOOMLINK_WARNING_FLAGS} ${BOOMLINK_WERROR_FLAGS})
  add_test(NAME boomlink_config_service_test COMMAND boomlink_config_service_test)

  # The Nanopb-free boundary (boomlink.md section 9), checked as a TEST rather
  # than only as a CI step, so `task test` runs it and a developer finds out
  # before pushing. The target structure enforces the common case at compile
  # time, but two routes get past that: re-adding the link to make something
  # build (the compile error just disappears), and a RELATIVE include reaching
  # nanopb's headers with no CMake change at all - which emits no symbol if it
  # only uses a macro or typedef, so an archive check cannot see it either. The
  # script checks symbols AND the compiler's own include list; see its header.
  #
  # Both lists are passed from here rather than hardcoded in the workflow. An
  # earlier version of this comment said a target added by a later phase would be
  # "one line away from being covered", and that was wrong twice over: the script
  # took exactly ONE source (`shift 4`), so the engine's four translation units
  # could not be added at all, and the engine target's own comment claimed it
  # inherited the property transitively. Verified: a relative
  # `#include "../../../bom-stm32node/third_party/nanopb/pb.h"` in
  # boomlink_link.c left all twelve tests green.
  #
  # So every translation unit that must hold the property is named, and so is
  # every archive. The engine's four sources are listed individually rather than
  # globbed: a glob silently covers a new file, which sounds good until the new
  # file is one that legitimately needs a different rule, and there would be
  # nothing here saying which files this claim is about.
  add_test(NAME boomlink_no_nanopb_dependency
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_no_nanopb.sh
            ${Python3_EXECUTABLE}
            ${BOOMLINK_NANOPB_DIR}
            ${CMAKE_CURRENT_BINARY_DIR}/compile_commands.json
            --sources
            ${CMAKE_CURRENT_SOURCE_DIR}/linkframe/boomlink_linkframe.c
            ${CMAKE_CURRENT_SOURCE_DIR}/linkengine/boomlink_port.c
            ${CMAKE_CURRENT_SOURCE_DIR}/linkengine/boomlink_dupcache.c
            ${CMAKE_CURRENT_SOURCE_DIR}/linkengine/boomlink_txqueue.c
            ${CMAKE_CURRENT_SOURCE_DIR}/linkengine/boomlink_link.c
            --artefacts
            $<TARGET_FILE:boomlink_linkframe>
            $<TARGET_FILE:boomlink_linkengine>
            $<TARGET_FILE:boomlink_linkframe_tool>
  )

  # The encoder's compile-time output bound, in BOTH languages (see
  # tests/check_encoder_bound.sh). Neither bound is exercised by any target in
  # either build: every library and tool here is C and none of them passes an
  # undersized buffer, and the C++ caller - the firmware's radio layer - does not
  # exist yet. Verified that with these two tests absent, BOTH bounds can be
  # deleted outright with the whole suite, the ARM build and ruff staying green,
  # while an intra-object overflow (the case AddressSanitizer is blind to) then
  # compiles clean under -Werror. These tests are the only thing standing between
  # the package's headline safety construct and silently becoming decoration.
  #
  # The std flags are passed explicitly rather than left to each compiler's
  # default, to match the standards the real callers are built with:
  # CMAKE_C_STANDARD 11 above, and fw/bom-stm32node/CMakeLists.txt's
  # CMAKE_CXX_STANDARD 17.
  #
  # The last argument is the diagnostic each language's bound is expected to
  # produce, so a rejection for an unrelated reason (a typo, a missing header, a
  # wrong -I) cannot masquerade as the bound working.
  add_test(NAME boomlink_linkframe_c_bound
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_encoder_bound.sh
            ${CMAKE_C_COMPILER}
            -std=c11
            ${CMAKE_CURRENT_SOURCE_DIR}/linkframe
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/encoder_bound_ok.c
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/encoder_bound_too_small.c
            "stringop-overflow|array-bounds|array argument is too small"
  )
  add_test(NAME boomlink_linkframe_cxx_bound
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_encoder_bound.sh
            ${CMAKE_CXX_COMPILER}
            -std=c++17
            ${CMAKE_CURRENT_SOURCE_DIR}/linkframe
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/encoder_bound_ok.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/encoder_bound_too_small.cpp
            "static assert"
  )

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
  # PYTHONPATH carries both the generated protobuf classes and linkframe/, so
  # the tests can import boomlink_linkframe - the independent Python reference
  # implementation of section 7.3's header. It lives next to the C rather than
  # under tests/ because PR 3's scope calls for a host parser as a deliverable
  # and PR 5's stm32node-cli will consume it.
  set_tests_properties(boomlink_python_tests PROPERTIES
    ENVIRONMENT "BOOMLINK_CODEC_TOOL=$<TARGET_FILE:boomlink_codec_tool>;BOOMLINK_LINKFRAME_TOOL=$<TARGET_FILE:boomlink_linkframe_tool>;PYTHONPATH=${BOOMLINK_PYTHON_PB_DIR}:${CMAKE_CURRENT_SOURCE_DIR}/linkframe"
  )
endif()

cmake_minimum_required(VERSION 3.20)

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

# Nanopb is vendored once, under bom-stm32node/third_party/ (git submodule
# pinned to release 0.4.9), and referenced from here by relative path rather
# than duplicating the submodule: both this host-native build and the ARM
# firmware build need the exact same runtime/generator.
set(NANOPB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../bom-stm32node/third_party/nanopb)
set(NANOPB_GENERATOR ${NANOPB_DIR}/generator/nanopb_generator.py)

if(NOT EXISTS ${NANOPB_GENERATOR})
  message(FATAL_ERROR
    "Nanopb submodule not found at ${NANOPB_DIR}. Run: "
    "git submodule update --init fw/bom-stm32node/third_party/nanopb")
endif()

add_library(nanopb STATIC
  ${NANOPB_DIR}/pb_common.c
  ${NANOPB_DIR}/pb_encode.c
  ${NANOPB_DIR}/pb_decode.c
)
target_include_directories(nanopb PUBLIC ${NANOPB_DIR})

# The Nanopb generator shells out to protoc (via grpcio-tools' bundled copy,
# falling back to a system install) and needs the `protobuf` package to parse
# what protoc produces - see fw/bom-stm32node/third_party/nanopb/requirements.txt.
# Checked at configure time so a missing dependency is one clear error instead
# of a confusing generator stack trace mid-build.
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
    "(matching fw/common/boomlink/pyproject.toml) with:\n"
    "    pip install protobuf grpcio-tools\n"
    "or run `task setup` in fw/common/boomlink.")
endif()

set(BOOMLINK_PROTO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/proto)
set(BOOMLINK_OPTIONS_FILE ${CMAKE_CURRENT_SOURCE_DIR}/nanopb/boomlink.options)
set(BOOMLINK_PROTO_NAMES common header system envelope)
list(TRANSFORM BOOMLINK_PROTO_NAMES PREPEND ${BOOMLINK_PROTO_DIR}/ OUTPUT_VARIABLE BOOMLINK_PROTO_FILES)
list(TRANSFORM BOOMLINK_PROTO_FILES APPEND .proto)

set(BOOMLINK_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${BOOMLINK_GENERATED_DIR})

set(BOOMLINK_GENERATED_SOURCES "")
foreach(proto_name IN LISTS BOOMLINK_PROTO_NAMES)
  set(out_c ${BOOMLINK_GENERATED_DIR}/${proto_name}.pb.c)
  set(out_h ${BOOMLINK_GENERATED_DIR}/${proto_name}.pb.h)
  # Every generated file depends on all four .proto sources (not just its
  # own), rather than tracking envelope.proto's imports precisely: simpler,
  # and the cost of an occasional unnecessary regeneration of these four tiny
  # files is not worth the extra bookkeeping.
  add_custom_command(
    OUTPUT ${out_c} ${out_h}
    COMMAND ${Python3_EXECUTABLE} ${NANOPB_GENERATOR}
            -I${BOOMLINK_PROTO_DIR}
            -f${BOOMLINK_OPTIONS_FILE}
            -D${BOOMLINK_GENERATED_DIR}
            ${BOOMLINK_PROTO_DIR}/${proto_name}.proto
    DEPENDS ${BOOMLINK_PROTO_FILES} ${BOOMLINK_OPTIONS_FILE} ${NANOPB_GENERATOR}
    COMMENT "Generating Nanopb code for ${proto_name}.proto"
    VERBATIM
  )
  list(APPEND BOOMLINK_GENERATED_SOURCES ${out_c})
endforeach()

add_custom_target(boomlink_proto_gen DEPENDS ${BOOMLINK_GENERATED_SOURCES})

# Isolated the same way PR 1's radio_layer isolates RadioLib (see
# fw/bom-stm32node/CMakeLists.txt): consuming code only ever sees
# boomlink_codec.h + the generated *.pb.h headers it re-exports, never
# Nanopb's own headers directly, and never hand-writes pb_encode/pb_decode
# calls outside this library.
add_library(boomlink_protocol STATIC
  ${BOOMLINK_GENERATED_SOURCES}
  ${CMAKE_CURRENT_SOURCE_DIR}/boomlink_codec.c
)
add_dependencies(boomlink_protocol boomlink_proto_gen)
target_include_directories(boomlink_protocol PUBLIC
  ${BOOMLINK_GENERATED_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(boomlink_protocol PUBLIC nanopb)

if(BOOMLINK_STANDALONE)
  enable_testing()

  add_executable(boomlink_codec_tool tests/test_encode_decode.c)
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

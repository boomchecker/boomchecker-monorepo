cmake_minimum_required(VERSION 3.16)
project(peak_detector C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(peak SHARED
    csrc/peak_detector.c
)
target_include_directories(peak PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/csrc
)

set_target_properties(peak PROPERTIES
    OUTPUT_NAME "peak"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build"
)

enable_testing()

add_executable(peak_tests
    csrc/tests/peak_detector_test.c
    csrc/peak_detector.c
    third_party/Unity/src/unity.c
)
target_include_directories(peak_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/csrc
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/Unity/src
)
target_compile_definitions(peak_tests PRIVATE PEAK_DETECTOR_TESTING=1)
set_target_properties(peak_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build"
)
add_test(NAME peak_tests COMMAND peak_tests)

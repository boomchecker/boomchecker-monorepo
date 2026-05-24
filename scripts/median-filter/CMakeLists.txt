cmake_minimum_required(VERSION 3.16)
project(peak_detector C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FetchContent)

add_library(peak SHARED
    csrc/peak_detector.c
    csrc/peak_detector_runner.c
)
target_include_directories(peak PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/csrc
)
target_link_libraries(peak PUBLIC m)

set_target_properties(peak PROPERTIES
    OUTPUT_NAME "peak"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build"
)

enable_testing()

FetchContent_Declare(Unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG v2.5.2
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(Unity)

set(UNITY_SRC_DIR "${unity_SOURCE_DIR}")
if(NOT EXISTS "${UNITY_SRC_DIR}/src/unity.c")
    message(FATAL_ERROR "Unity not fetched; expected ${UNITY_SRC_DIR}/src/unity.c")
endif()

set(UNITY_SRC ${UNITY_SRC_DIR}/src/unity.c)
set(UNITY_INCLUDE_DIR ${UNITY_SRC_DIR}/src)

add_executable(peak_tests
    csrc/tests/peak_detector_test.c
    csrc/peak_detector.c
    csrc/peak_detector_runner.c
    ${UNITY_SRC}
)
target_include_directories(peak_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/csrc
    ${UNITY_INCLUDE_DIR}
)
target_compile_definitions(peak_tests PRIVATE PEAK_DETECTOR_TESTING=1)
target_link_libraries(peak_tests PRIVATE m)
set_target_properties(peak_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build"
)

add_executable(peak_runner_tests
    csrc/tests/peak_detector_runner_test.c
    csrc/peak_detector.c
    csrc/peak_detector_runner.c
    ${UNITY_SRC}
)
target_include_directories(peak_runner_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/csrc
    ${UNITY_INCLUDE_DIR}
)
target_compile_definitions(peak_runner_tests PRIVATE PEAK_DETECTOR_TESTING=1)
target_link_libraries(peak_runner_tests PRIVATE m)
set_target_properties(peak_runner_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build"
)

add_test(NAME peak_tests COMMAND peak_tests)
add_test(NAME peak_runner_tests COMMAND peak_runner_tests)

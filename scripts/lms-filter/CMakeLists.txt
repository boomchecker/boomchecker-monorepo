cmake_minimum_required(VERSION 3.16)
project(fxlms_filter C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(GENERATED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/build/generated")

add_library(lms_filter SHARED
    csrc/lms_filter.c
    csrc/lms_filter_host.c
)
target_include_directories(lms_filter PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/csrc
    ${GENERATED_DIR}
)
set_target_properties(lms_filter PROPERTIES
    OUTPUT_NAME "lms_filter"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build"
)

enable_testing()

add_executable(lms_filter_tests
    csrc/lms_filter.c
    csrc/tests/lms_filter_test.c
)
target_include_directories(lms_filter_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/csrc
    ${GENERATED_DIR}
)
target_compile_definitions(lms_filter_tests PRIVATE FXLMS_TESTING)
set_target_properties(lms_filter_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build"
)
add_test(NAME lms_filter_tests COMMAND lms_filter_tests)

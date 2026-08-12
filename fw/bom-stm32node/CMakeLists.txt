cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME bom-stm32node)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM, C and C++ languages. C++ is needed only for
# the RadioLib SX1262 driver and its thin STM32 HAL adapter under App/radio/;
# the rest of the firmware stays C (see App/radio/radio.h).
enable_language(C CXX ASM)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# The PDM->PCM DSP chain (CIC5 + FIR) must finish one ring half (~21.3 ms of
# audio) faster than real time or the mic overruns - true regardless of
# CMAKE_BUILD_TYPE. At the project-wide Debug -O0, it measurably does not
# (disassembly showed ~2-3x over budget). Force this one hot TU to -O2 so
# Debug builds stay debuggable everywhere else but this loop still keeps up.
set_source_files_properties(${CMAKE_SOURCE_DIR}/Core/Src/pdm_pcm.c
    PROPERTIES COMPILE_OPTIONS "-O2"
)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# RadioLib (SX1262 driver), vendored as a git submodule pinned to tag 7.7.1
# under third_party/RadioLib - see fw/bom-stm32node/third_party/RadioLib.
# RADIOLIB_STATIC_ONLY avoids heap use in RadioLib's own SPI transfer helper,
# matching this firmware's static-allocation-only policy for protocol/radio
# code (see docs/firmware/bom-stm32node/boomlink.md section 5.2/16).
add_subdirectory(third_party/RadioLib)
target_compile_definitions(RadioLib PRIVATE RADIOLIB_STATIC_ONLY=1)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    ${CMAKE_SOURCE_DIR}/Core/Src/pdm_pcm.c
    ${CMAKE_SOURCE_DIR}/Core/Src/mic.c
    ${CMAKE_SOURCE_DIR}/Core/Src/cli.c
    ${CMAKE_SOURCE_DIR}/Core/Src/usb_cli.c
    ${CMAKE_SOURCE_DIR}/Core/Src/pcm_stream.c
    ${CMAKE_SOURCE_DIR}/third_party/embedded-cli/lib/src/embedded_cli.c
    ${CMAKE_SOURCE_DIR}/App/radio/radio.cpp
    ${CMAKE_SOURCE_DIR}/App/radio/e22_radio.cpp
    ${CMAKE_SOURCE_DIR}/App/radio/stm32_radiolib_hal.cpp
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    ${CMAKE_SOURCE_DIR}/Core/Inc
    ${CMAKE_SOURCE_DIR}/third_party/embedded-cli/lib/include
    ${CMAKE_SOURCE_DIR}/App/radio
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    RadioLib

    # Add user defined libraries
)

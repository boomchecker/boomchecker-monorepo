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

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# The PDM->PCM DSP chain (CIC5 + FIR) must finish one ring half (~21.3 ms of
# audio) faster than real time or the mic overruns - true regardless of
# CMAKE_BUILD_TYPE. At the project-wide Debug -O0, it measurably does not
# (disassembly showed ~2-3x over budget). Force this one hot TU to -O2 so
# Debug builds stay debuggable everywhere else but this loop still keeps up.
set_source_files_properties(${CMAKE_SOURCE_DIR}/Core/Src/pdm_pcm.c
    PROPERTIES COMPILE_OPTIONS "-O2"
)

# Same reasoning for the detector's DSP: the MFCC (1024-pt FFT every 32 ms)
# shares the real-time budget with pdm_pcm, so keep the math TUs at -O2.
set_source_files_properties(
    ${CMAKE_SOURCE_DIR}/Core/Src/detector.c
    ${CMAKE_SOURCE_DIR}/Core/Src/mfcc_processor.c
    ${CMAKE_SOURCE_DIR}/Core/Src/svm_classifier.c
    ${CMAKE_SOURCE_DIR}/third_party/CMSIS-DSP/Source/BasicMathFunctions/BasicMathFunctions.c
    ${CMAKE_SOURCE_DIR}/third_party/CMSIS-DSP/Source/CommonTables/CommonTables.c
    ${CMAKE_SOURCE_DIR}/third_party/CMSIS-DSP/Source/ComplexMathFunctions/ComplexMathFunctions.c
    ${CMAKE_SOURCE_DIR}/third_party/CMSIS-DSP/Source/FastMathFunctions/FastMathFunctions.c
    ${CMAKE_SOURCE_DIR}/third_party/CMSIS-DSP/Source/MatrixFunctions/MatrixFunctions.c
    ${CMAKE_SOURCE_DIR}/third_party/CMSIS-DSP/Source/StatisticsFunctions/StatisticsFunctions.c
    ${CMAKE_SOURCE_DIR}/third_party/CMSIS-DSP/Source/SupportFunctions/SupportFunctions.c
    ${CMAKE_SOURCE_DIR}/third_party/CMSIS-DSP/Source/TransformFunctions/TransformFunctions.c
    PROPERTIES COMPILE_OPTIONS "-O2"
)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

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
    ${CMAKE_SOURCE_DIR}/Core/Src/detector.c
    ${CMAKE_SOURCE_DIR}/Core/Src/mfcc_processor.c
    ${CMAKE_SOURCE_DIR}/Core/Src/svm_classifier.c
    ${CMAKE_SOURCE_DIR}/Core/Src/dfu_boot.c
    ${CMAKE_SOURCE_DIR}/third_party/embedded-cli/lib/src/embedded_cli.c
    # CMSIS-DSP: per-group combined TUs (only what arm_mfcc_f32 + the SVM
    # need); --gc-sections drops the unused tables/kernels.
    ${CMAKE_SOURCE_DIR}/third_party/CMSIS-DSP/Source/BasicMathFunctions/BasicMathFunctions.c
    ${CMAKE_SOURCE_DIR}/third_party/CMSIS-DSP/Source/CommonTables/CommonTables.c
    ${CMAKE_SOURCE_DIR}/third_party/CMSIS-DSP/Source/ComplexMathFunctions/ComplexMathFunctions.c
    ${CMAKE_SOURCE_DIR}/third_party/CMSIS-DSP/Source/FastMathFunctions/FastMathFunctions.c
    ${CMAKE_SOURCE_DIR}/third_party/CMSIS-DSP/Source/MatrixFunctions/MatrixFunctions.c
    ${CMAKE_SOURCE_DIR}/third_party/CMSIS-DSP/Source/StatisticsFunctions/StatisticsFunctions.c
    ${CMAKE_SOURCE_DIR}/third_party/CMSIS-DSP/Source/SupportFunctions/SupportFunctions.c
    ${CMAKE_SOURCE_DIR}/third_party/CMSIS-DSP/Source/TransformFunctions/TransformFunctions.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    ${CMAKE_SOURCE_DIR}/Core/Inc
    ${CMAKE_SOURCE_DIR}/third_party/embedded-cli/lib/include
    ${CMAKE_SOURCE_DIR}/third_party/CMSIS-DSP/Include
    ${CMAKE_SOURCE_DIR}/third_party/CMSIS-DSP/PrivateInclude
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

    # Add user defined libraries
)

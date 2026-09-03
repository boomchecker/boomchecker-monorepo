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

# RadioLib (SX1262 driver), vendored as a git submodule pinned to tag 7.7.1
# under third_party/RadioLib - see fw/bom-stm32node/third_party/RadioLib.
# RADIOLIB_STATIC_ONLY avoids heap use in RadioLib's own SPI transfer helper,
# matching this firmware's static-allocation-only policy for protocol/radio
# code (see docs/firmware/bom-stm32node/boomlink.md section 5.2/16). PUBLIC,
# not PRIVATE: the macro is not confined to RadioLib's own .cpp files - it
# also toggles member layout in some of its header-only classes (e.g.
# AX25Frame, RadioLibBCH, ITA2String). Consuming code that includes
# <RadioLib.h> without seeing the same value would build against a
# different layout than the library was compiled with for those classes -
# harmless while nothing here uses them, but a silent ODR violation the
# moment something does.
#
# The RADIOLIB_EXCLUDE_* defines drop every module/protocol family this
# firmware never uses (only SX126x/SX1262 is instantiated - see
# App/radio/radio.cpp). Each excluded macro wraps the ENTIRE body of its
# .cpp file(s) in an #if, so the preprocessor discards nearly all of it
# before the compiler does any real work - this cuts RadioLib from ~180
# compiled translation units down to a couple of dozen, which matters most
# on every CI run (test-bom-stm32node-build.yaml). RADIOLIB_EXCLUDE_SX126X
# and RADIOLIB_EXCLUDE_STM32WLX are deliberately NOT set: SX126X is what we
# use, and STM32WLX is already auto-excluded on any non-STM32WL target.
add_subdirectory(third_party/RadioLib)
target_compile_definitions(RadioLib PUBLIC
    RADIOLIB_STATIC_ONLY=1
    RADIOLIB_EXCLUDE_CC1101=1
    RADIOLIB_EXCLUDE_NRF24=1
    RADIOLIB_EXCLUDE_RF69=1
    RADIOLIB_EXCLUDE_SX1231=1
    RADIOLIB_EXCLUDE_SI443X=1
    RADIOLIB_EXCLUDE_RFM2X=1
    RADIOLIB_EXCLUDE_SX127X=1
    RADIOLIB_EXCLUDE_SX128X=1
    RADIOLIB_EXCLUDE_LR11X0=1
    RADIOLIB_EXCLUDE_LR2021=1
    RADIOLIB_EXCLUDE_AFSK=1
    RADIOLIB_EXCLUDE_AX25=1
    RADIOLIB_EXCLUDE_HELLSCHREIBER=1
    RADIOLIB_EXCLUDE_MORSE=1
    RADIOLIB_EXCLUDE_RTTY=1
    RADIOLIB_EXCLUDE_SSTV=1
    RADIOLIB_EXCLUDE_BELL=1
    RADIOLIB_EXCLUDE_APRS=1
    RADIOLIB_EXCLUDE_LORAWAN=1
    RADIOLIB_EXCLUDE_FSK4=1
    RADIOLIB_EXCLUDE_PAGER=1
    RADIOLIB_EXCLUDE_ADSB=1
    RADIOLIB_EXCLUDE_DIRECT_RECEIVE=1
)

# The radio layer (App/radio/) as its own static library, linked PRIVATEly
# against RadioLib: this is what actually enforces "no RadioLib types leak
# into application code" (boomlink.md section 5.1/16 rule 3) as a compiler
# error rather than a comment. Only this library's own translation units
# (radio.cpp, e22_radio.cpp, stm32_radiolib_hal.cpp) ever see RadioLib's
# headers; PRIVATE linkage means that include path is NOT propagated to
# whatever links against radio_layer. Its own PUBLIC include directory
# exposes radio.h (the only header application code should include) - and
# incidentally e22_radio.h/stm32_radiolib_hal.hpp too, since CMake's
# per-target include dirs aren't file-granular, but those both immediately
# fail to compile outside this library anyway (they include <RadioLib.h>,
# which is exactly the path that stops here).
add_library(radio_layer STATIC
    ${CMAKE_SOURCE_DIR}/App/radio/radio.cpp
    ${CMAKE_SOURCE_DIR}/App/radio/e22_radio.cpp
    ${CMAKE_SOURCE_DIR}/App/radio/stm32_radiolib_hal.cpp
)
target_include_directories(radio_layer PUBLIC ${CMAKE_SOURCE_DIR}/App/radio)
target_link_libraries(radio_layer PRIVATE RadioLib stm32cubemx)

# BoomProtocol/Nanopb foundation (docs/firmware/bom-stm32node/boomlink.md PR 2):
# .proto schema, generated Nanopb codec and boomlink_codec.h/.c live in
# fw/common/boomlink so the host-native test build (fw/common/boomlink's own
# CMakePresets.json) and this ARM cross build share one copy instead of
# duplicating the generation logic. Produces `boomlink_protocol`,
# `boomlink_linkframe` and `boomlink_linkengine` (all linked below - App/link/
# is their call site). No test targets are built under add_subdirectory() -
# see fw/common/boomlink/CMakeLists.txt for why.
add_subdirectory(${CMAKE_SOURCE_DIR}/../common/boomlink boomlink)

# The config storage wrapper's real flash backend (App/storage/, PR 4
# Phase B) - a separate, STM32-specific static library rather than a source
# added straight into ${CMAKE_PROJECT_NAME}, the same isolation radio_layer
# gets: this is the one place HAL_FLASH_*/HAL_FLASHEx_* calls exist in this
# firmware. Linked into ${CMAKE_PROJECT_NAME} below (PR 4 Phase C): App/
# protocol/protocol_service.c's protocol_service_load_config() is the first
# real caller of boomlink_config_store_load() at boot.
add_library(boomlink_flash_storage_port STATIC
    ${CMAKE_SOURCE_DIR}/App/storage/boomlink_flash_storage_port.c
)
target_include_directories(boomlink_flash_storage_port PUBLIC ${CMAKE_SOURCE_DIR}/App/storage)
target_link_libraries(boomlink_flash_storage_port PUBLIC boomlink_storage_port PRIVATE stm32cubemx)

# The link engine's radio port adapter + call site (App/link/, PR 3 Phase C) -
# plain C, no RadioLib types visible here (it calls through App/radio/radio.h,
# same rule as everything else outside radio_layer).
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/App/link/boomlink_radio_port.c
    ${CMAKE_SOURCE_DIR}/App/link/link_service.c
)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/App/link)

# The protocol dispatcher's firmware call site (App/protocol/, PR 4 Phase C) -
# wires fw/common/boomlink's dispatch/command_service/config_service onto
# the link engine above and the real flash-backed config store. Plain C,
# same "no RadioLib types outside radio_layer" rule; talks to radio.h only
# through its own public API (self_test/clear_statistics/request_
# diagnostics), never RadioLib directly.
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/App/protocol/protocol_service.c
)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/App/protocol)

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
    ${CMAKE_SOURCE_DIR}/Core/Src/gps.c
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
    radio_layer
    boomlink_protocol
    boomlink_linkengine  # PUBLIC-links boomlink_linkframe too, see fw/common/boomlink/CMakeLists.txt

    # PR 4 Phase C: the protocol dispatcher and its command/config services
    # (fw/common/boomlink/CMakeLists.txt) plus the real flash-backed config
    # store, all wired together by App/protocol/protocol_service.c above.
    # boomlink_dispatch (PUBLIC-linked by both of the first two already,
    # see that file) and boomlink_storage_port (PUBLIC-linked by both of
    # the last two) do not need listing again here.
    boomlink_command_service
    boomlink_config_service
    boomlink_config_store
    boomlink_flash_storage_port

    # Fleet discovery (boomlink.md section 8.6) - Ping/Pong and Wakeup/
    # WakeupResponse, wired the same way as the two services above.
    boomlink_system_service

    # Add user defined libraries
)

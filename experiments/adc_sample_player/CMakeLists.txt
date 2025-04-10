cmake_minimum_required(VERSION 3.13...3.27)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(EXECUTABLE_NAME adc_sample_player)
# PICO_BOARD is set later, after Musin is added

set(EXAMPLES_SUPPORT_PATH ${CMAKE_CURRENT_LIST_DIR}/../support/)
set(MUSIN_PATH ${CMAKE_CURRENT_LIST_DIR}/../../musin)

# --- Pico SDK and Toolchain Setup ---
# Define the SDK path *first*
set(PICO_SDK_PATH ${MUSIN_PATH}/ports/pico/pico-sdk CACHE PATH "Path to the Pico SDK")
set(PICO_SDK_EXTRAS_PATH ${MUSIN_PATH}/ports/pico/pico-extras/)

# Set the specific path for the toolchain executables (used by the toolchain file)
# Note: The path should point to the root of the toolchain, not the bin directory itself.
# The toolchain file expects to find 'bin/arm-none-eabi-gcc', etc. relative to this path.
set(PICO_TOOLCHAIN_PATH /Applications/ArmGNUToolchain/14.2.rel1/arm-none-eabi CACHE PATH "Path to ARM GCC toolchain binaries")
# --- End Pico SDK and Toolchain Setup ---

# Import SDK definitions *after* init
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

# Project definition - Now CMake knows to use the toolchain file for compiler detection
project(${EXECUTABLE_NAME} C CXX ASM) # Add ASM language for SDK compatibility
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

# Add the Musin library subdirectory *first* so it can set PICO_BOARD_HEADER_DIRS
# Musin's CMakeLists should *not* call pico_sdk_init() itself.
add_subdirectory(${MUSIN_PATH} musin_build)

# Set the board *after* Musin has added its board directory
set(PICO_BOARD dato_submarine)


# Define the executable
add_executable(${EXECUTABLE_NAME}
  main.cpp
  ${EXAMPLES_SUPPORT_PATH}/samples/AudioSampleKickc78_16bit_44kw.cpp
  ${EXAMPLES_SUPPORT_PATH}/samples/AudioSampleHatdr55_16bit_44kw.cpp
  ${EXAMPLES_SUPPORT_PATH}/samples/AudioSampleSnare100_16bit_44kw.cpp
  ${EXAMPLES_SUPPORT_PATH}/samples/AudioSampleClapdr110_16bit_44kw.cpp
)

target_include_directories(${EXECUTABLE_NAME} PRIVATE
  ${EXAMPLES_SUPPORT_PATH}
)

target_compile_definitions(${EXECUTABLE_NAME} PUBLIC # Use PUBLIC to propagate to dependencies
  AUDIO_BLOCK_SAMPLES=32
  PICO_STDIO_USB_ENABLE_RESET_VIA_VENDOR_INTERFACE=1
)

# Link against required libraries
target_link_libraries(${EXECUTABLE_NAME} PRIVATE
    pico_stdlib       # Base SDK library
    musin::core       # Core Musin utilities
    musin::hal        # For AnalogInMux
    musin::drivers    # For audio codec (aic3204) used by musin::audio
    musin::audio      # For audio processing (Mixer, Sound, etc.)
    musin::usb_midi   # For USB MIDI input/output
    musin::filesystem # For filesystem
    # Add any other direct SDK dependencies if needed, though most should be transitive
)

if(SWAP_AUDIO_CLOCK)
  target_compile_definitions(${EXECUTABLE_NAME} PRIVATE
    PICO_AUDIO_I2S_CLOCK_PINS_SWAPPED=1
  )
endif(SWAP_AUDIO_CLOCK)

pico_enable_filesystem(${EXECUTABLE_NAME})
pico_add_extra_outputs(${EXECUTABLE_NAME})

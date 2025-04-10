cmake_minimum_required(VERSION 3.13...3.27)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PICO_BOARD dato_submarine)

set(EXECUTABLE_NAME adc_sample_player)

set(EXAMPLES_SUPPORT_PATH ${CMAKE_CURRENT_LIST_DIR}/../support/)
set(MUSIN_PATH ${CMAKE_CURRENT_LIST_DIR}/../../musin)

# Project definition first
project(${EXECUTABLE_NAME} C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

set(PICO_BOARD dato_submarine)

# Add the Musin library subdirectory *first* so it can set PICO_BOARD_HEADER_DIRS
add_subdirectory(${MUSIN_PATH} musin_build)

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

target_compile_definitions(${EXECUTABLE_NAME} PRIVATE
  AUDIO_BLOCK_SAMPLES=32
  # Add TinyUSB reset definition required by musin::usb_midi usage
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
    # Add any other direct SDK dependencies if needed, though most should be transitive
)

if(SWAP_AUDIO_CLOCK)
  target_compile_definitions(${EXECUTABLE_NAME} PRIVATE
    PICO_AUDIO_I2S_CLOCK_PINS_SWAPPED=1
  )
endif(SWAP_AUDIO_CLOCK)

# Add standard Pico outputs (UF2, etc.)
pico_add_extra_outputs(${EXECUTABLE_NAME})
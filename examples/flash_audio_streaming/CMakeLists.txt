cmake_minimum_required(VERSION 3.13...3.27)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(EXECUTABLE_NAME flash_audio_streaming)

set(EXAMPLES_SUPPORT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../support/)
set(SDK_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../musin/ports/pico/pico-sdk/)
set(SDK_EXTRAS_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../musin/ports/pico/pico-extras/)

# initialize pico-sdk from submodule
# note: this must happen before project()
include(${SDK_PATH}/pico_sdk_init.cmake)
include(${SDK_EXTRAS_PATH}/external/pico_extras_import.cmake)

set(LIB_DIR ${CMAKE_CURRENT_LIST_DIR}/../../musin/ports/pico/libraries)

project(${EXECUTABLE_NAME})
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

add_subdirectory(${LIB_DIR}/pico-vfs build)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../musin/musin_init.cmake)

add_executable(${EXECUTABLE_NAME}
  main.cpp
  ${MUSIN_ROOT}/shared/audio/audio_output.cpp
  ${MUSIN_ROOT}/shared/audio/pitch_shifter.cpp
  ${MUSIN_ROOT}/shared/audio/audio_memory_reader.cpp
  ${MUSIN_ROOT}/shared/audio/data_ulaw.c
  ${MUSIN_ROOT}/shared/audio/mixer.cpp
  ${MUSIN_ROOT}/shared/filesystem/filesystem.c
  ${MUSIN_ROOT}/shared/midi/midi_wrapper.cpp
  ${MUSIN_ROOT}/shared/pico_uart.cpp
  ${MUSIN_ROOT}/shared/usb/usb.cpp
  ${MUSIN_ROOT}/shared/usb/usb_descriptors.c
  ${MUSIN_ROOT}/shared/usb/midi_usb_bridge/MIDIUSB.cpp
  ${EXAMPLES_SUPPORT_PATH}/samples/AudioSampleKick.cpp
  ${EXAMPLES_SUPPORT_PATH}/samples/AudioSampleSnare.cpp
  ${EXAMPLES_SUPPORT_PATH}/samples/AudioSampleGong.cpp
  ${EXAMPLES_SUPPORT_PATH}/samples/AudioSampleCashregister.cpp
  ${EXAMPLES_SUPPORT_PATH}/samples/AudioSampleHihat.cpp
)

target_include_directories(${EXECUTABLE_NAME} PRIVATE
  ${MUSIN_ROOT}/shared
  ${MUSIN_ROOT}/shared/usb
  ${MUSIN_ROOT}/shared/usb/midi_usb_bridge
  ${LIB_DIR}/arduino_midi_library/src
  ${LIB_DIR}/Arduino-USBMIDI/src
)

add_definitions(-DAUDIO_BLOCK_SAMPLES=128)

target_compile_definitions(${EXECUTABLE_NAME} PRIVATE
  PICO_AUDIO_I2S_MONO_INPUT=1
  USE_AUDIO_I2S=1
  PICO_AUDIO_I2S_DATA_PIN=18
  PICO_AUDIO_I2S_CLOCK_PIN_BASE=16
)


target_link_libraries(${EXECUTABLE_NAME} PRIVATE
  pico_stdlib
  tinyusb_device
  tinyusb_board
  hardware_pio
  hardware_irq
  pico_audio_i2s
)


pico_enable_stdio_usb(${EXECUTABLE_NAME} 0)
pico_enable_stdio_uart(${EXECUTABLE_NAME} 0)
pico_add_extra_outputs(${EXECUTABLE_NAME})
pico_enable_filesystem(${EXECUTABLE_NAME})

# Disable warnings since pico_audio_i2s_32b causes warnings, which error because of -Werror in pico-vfs.
target_compile_options(${EXECUTABLE_NAME} PRIVATE -w)


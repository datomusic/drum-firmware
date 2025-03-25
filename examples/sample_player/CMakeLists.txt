cmake_minimum_required(VERSION 3.13...3.27)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(EXECUTABLE_NAME sample_player)

set(EXAMPLES_SUPPORT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../support/)
set(SDK_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../musin/ports/pico/pico-sdk/)
set(SDK_EXTRAS_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../musin/ports/pico/pico-extras/)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../musin/musin_init.cmake)
# initialize pico-sdk from submodule
# note: this must happen before project()
include(${SDK_PATH}/pico_sdk_init.cmake)
include(${SDK_EXTRAS_PATH}/external/pico_extras_import.cmake)

project(${EXECUTABLE_NAME})
pico_sdk_init()

add_executable(${EXECUTABLE_NAME}
  main.cpp
  ${EXAMPLES_SUPPORT_PATH}/samples/AudioSampleKick.cpp
  ${EXAMPLES_SUPPORT_PATH}/samples/AudioSampleSnare.cpp
  ${EXAMPLES_SUPPORT_PATH}/samples/AudioSampleGong.cpp
  ${EXAMPLES_SUPPORT_PATH}/samples/AudioSampleCashregister.cpp
  ${EXAMPLES_SUPPORT_PATH}/samples/AudioSampleHihat.cpp
)

target_include_directories(${EXECUTABLE_NAME} PRIVATE
  ${EXAMPLES_SUPPORT_PATH}
)


target_compile_definitions(${EXECUTABLE_NAME} PRIVATE
  AUDIO_BLOCK_SAMPLES=32
  PICO_AUDIO_I2S_DATA_PIN=18
  PICO_AUDIO_I2S_CLOCK_PIN_BASE=16
)

musin_init(${EXECUTABLE_NAME})

pico_enable_stdio_usb(${EXECUTABLE_NAME} 1)
pico_enable_stdio_uart(${EXECUTABLE_NAME} 1)
pico_add_extra_outputs(${EXECUTABLE_NAME})

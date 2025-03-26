cmake_minimum_required(VERSION 3.13...3.27)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(EXECUTABLE_NAME sample_player)

set(EXAMPLES_SUPPORT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../support/)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../musin/musin_init.cmake)

project(${EXECUTABLE_NAME})

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
musin_init_audio(${EXECUTABLE_NAME})

pico_enable_stdio_usb(${EXECUTABLE_NAME} 0)
pico_enable_stdio_uart(${EXECUTABLE_NAME} 1)

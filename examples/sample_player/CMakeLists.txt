cmake_minimum_required(VERSION 3.13...3.27)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(EXECUTABLE_NAME sample_player)

set(MUSIN_SHARED ${CMAKE_CURRENT_SOURCE_DIR}/../../musin/shared/)
set(EXAMPLES_SUPPORT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../support/)
set(SDK_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../musin/ports/pico/pico-sdk/)
set(SDK_EXTRAS_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../musin/ports/pico/pico-extras/)

# initialize pico-sdk from submodule
# note: this must happen before project()
include(${SDK_PATH}/pico_sdk_init.cmake)
include(${SDK_EXTRAS_PATH}/external/pico_extras_import.cmake)


project(${EXECUTABLE_NAME})
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

add_executable(${EXECUTABLE_NAME}
    main.cpp
    ${MUSIN_SHARED}/audio/audio_output.cpp
    ${MUSIN_SHARED}/audio/pitch_shifter.cpp
    ${MUSIN_SHARED}/audio/audio_memory_reader.cpp
    ${MUSIN_SHARED}/audio/data_ulaw.c
    ${MUSIN_SHARED}/audio/mixer.cpp
    ${EXAMPLES_SUPPORT_PATH}/samples/AudioSampleKick.cpp
    ${EXAMPLES_SUPPORT_PATH}/samples/AudioSampleSnare.cpp
    ${EXAMPLES_SUPPORT_PATH}/samples/AudioSampleGong.cpp
    ${EXAMPLES_SUPPORT_PATH}/samples/AudioSampleCashregister.cpp
    ${EXAMPLES_SUPPORT_PATH}/samples/AudioSampleHihat.cpp
)

target_include_directories(${EXECUTABLE_NAME} PRIVATE
    ${MUSIN_SHARED}
    ${EXAMPLES_SUPPORT_PATH}
)

add_definitions(-DAUDIO_BLOCK_SAMPLES=32)

target_compile_definitions(${EXECUTABLE_NAME} PRIVATE
  PICO_AUDIO_I2S_MONO_INPUT=1
  USE_AUDIO_I2S=1
  PICO_AUDIO_I2S_DATA_PIN=18
  PICO_AUDIO_I2S_CLOCK_PIN_BASE=16
)

target_link_libraries(${EXECUTABLE_NAME}
  pico_stdlib
  hardware_dma
  hardware_pio
  hardware_irq
  pico_audio_i2s
)

pico_enable_stdio_usb(${EXECUTABLE_NAME} 1)
pico_enable_stdio_uart(${EXECUTABLE_NAME} 1)
pico_add_extra_outputs(${EXECUTABLE_NAME})

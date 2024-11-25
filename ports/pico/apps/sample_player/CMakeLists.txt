cmake_minimum_required(VERSION 3.13...3.27)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SDK_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../pico-sdk/)
set(SDK_EXTRAS_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../pico-extras/)

# initialize pico-sdk from submodule
# note: this must happen before project()
include(${SDK_PATH}/pico_sdk_init.cmake)
include(${SDK_EXTRAS_PATH}/external/pico_extras_import.cmake)


project(sample_player)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../pico_audio_i2s_32b/src/pico_audio_32b pico_audio_32b)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../pico_audio_i2s_32b pico_audio_i2s_32b)

add_executable(sample_player
    main.cpp
    audio_output.cpp
    timestretched/pitch_shifter.cpp
    timestretched/audio_memory_reader.cpp
    # timestretched/AudioSampleKick.cpp
    # timestretched/AudioSampleSnare.cpp
    timestretched/data_ulaw.c
)

add_definitions(-DAUDIO_BLOCK_SAMPLES=1024)

target_link_libraries(sample_player
  pico_stdlib
  pico_audio_32b
  pico_audio_i2s_32b
)

pico_enable_stdio_usb(sample_player 1)
pico_enable_stdio_uart(sample_player 1)

pico_add_extra_outputs(sample_player)

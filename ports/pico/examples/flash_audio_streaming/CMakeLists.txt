cmake_minimum_required(VERSION 3.13...3.27)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(EXECUTABLE_NAME flash_audio_streaming)

set(CORE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../core)
set(SDK_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../pico-sdk/)
set(SDK_EXTRAS_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../pico-extras/)

# initialize pico-sdk from submodule
# note: this must happen before project()
include(${SDK_PATH}/pico_sdk_init.cmake)
include(${SDK_EXTRAS_PATH}/external/pico_extras_import.cmake)


project(${EXECUTABLE_NAME})
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

add_subdirectory(../../libraries/pico-vfs build)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../pico_audio_i2s_32b/src/pico_audio_32b pico_audio_32b)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../pico_audio_i2s_32b pico_audio_i2s_32b)

add_executable(${EXECUTABLE_NAME}
    main.cpp
    ${CORE_PATH}/audio_output.cpp
    ${CORE_PATH}/timestretched/pitch_shifter.cpp
    ${CORE_PATH}/timestretched/audio_memory_reader.cpp
    ${CORE_PATH}/timestretched/AudioSampleSnare.cpp
    ${CORE_PATH}/timestretched/data_ulaw.c
    ${CORE_PATH}/teensy_audio/mixer.cpp
    ${CORE_PATH}/filesystem.c
)

target_include_directories(${EXECUTABLE_NAME} PRIVATE
    ${CORE_PATH}/../
    ${CORE_PATH}
)

add_definitions(-DAUDIO_BLOCK_SAMPLES=32)

target_link_libraries(${EXECUTABLE_NAME} PRIVATE
  pico_stdlib
  pico_audio_32b
  pico_audio_i2s_32b
)


pico_enable_stdio_usb(${EXECUTABLE_NAME} 1)
pico_enable_stdio_uart(${EXECUTABLE_NAME} 0)
pico_add_extra_outputs(${EXECUTABLE_NAME})
pico_enable_filesystem(${EXECUTABLE_NAME})

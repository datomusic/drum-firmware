cmake_minimum_required(VERSION 3.13...3.27)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SDK_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../pico-sdk/)
set(SDK_EXTRAS_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../pico-extras/)

# initialize pico-sdk from submodule
# note: this must happen before project()
include(${SDK_PATH}/pico_sdk_init.cmake)
include(${SDK_EXTRAS_PATH}/external/pico_extras_import.cmake)


project(sine_test)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../pico_audio_i2s_32b/src/pico_audio_32b pico_audio_32b)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../pico_audio_i2s_32b pico_audio_i2s_32b)

add_executable(sine_test
    main.cpp
)

target_link_libraries(sine_test
  pico_stdlib
  pico_audio_32b
  pico_audio_i2s_32b
)

pico_enable_stdio_usb(sine_test 1)
pico_enable_stdio_uart(sine_test 1)
pico_add_extra_outputs(sine_test)

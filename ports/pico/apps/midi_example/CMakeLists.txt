cmake_minimum_required(VERSION 3.13...3.27)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SDK_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../pico-sdk/)

# initialize pico-sdk from submodule
# note: this must happen before project()
include(${SDK_PATH}/pico_sdk_init.cmake)

project(midi_example)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

add_executable(midi_example
    main.cpp
)

target_link_libraries(midi_example
  pico_stdlib
)

pico_enable_stdio_usb(midi_example 1)
pico_enable_stdio_uart(midi_example 1)
pico_add_extra_outputs(midi_example)

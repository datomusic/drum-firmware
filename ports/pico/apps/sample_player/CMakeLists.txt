cmake_minimum_required(VERSION 3.13...3.27)

# initialize pico-sdk from submodule
# note: this must happen before project()
include(${CMAKE_CURRENT_SOURCE_DIR}/../../pico-sdk/pico_sdk_init.cmake)

project(sample_player)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

add_executable(sample_player
    main.cpp
)

target_link_libraries(sample_player
  pico_stdlib)

# Use USB instead of UART for stdio
pico_enable_stdio_usb(sample_player 1)
pico_enable_stdio_uart(sample_player 0)

pico_add_extra_outputs(sample_player)

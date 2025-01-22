cmake_minimum_required(VERSION 3.12)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(ws2812_blink C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(ws2812_blink)

pico_generate_pio_header(ws2812_blink ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio)

target_sources(ws2812_blink PRIVATE main.cpp)

target_link_libraries(ws2812_blink PRIVATE
    pico_stdlib
    hardware_pio
)

pico_add_extra_outputs(ws2812_blink)

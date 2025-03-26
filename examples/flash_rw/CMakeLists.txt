cmake_minimum_required(VERSION 3.13...3.27)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(EXECUTABLE_NAME flash_rw_demo)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../musin/musin_init.cmake)

project(${EXECUTABLE_NAME})

add_executable(${EXECUTABLE_NAME}
  main.cpp
)

musin_init(${EXECUTABLE_NAME})
musin_init_filesystem(${EXECUTABLE_NAME})

pico_enable_stdio_usb(${EXECUTABLE_NAME} 0)
pico_enable_stdio_uart(${EXECUTABLE_NAME} 0)

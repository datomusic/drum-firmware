cmake_minimum_required(VERSION 3.13...3.27)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(EXECUTABLE_NAME midi_example)

set(SDK_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../pico-sdk/)

set(EXAMPLES_SUPPORT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../support/)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../musin/musin_init.cmake)

project(${EXECUTABLE_NAME})

add_executable(${EXECUTABLE_NAME}
    main.cpp
)

target_include_directories(${EXECUTABLE_NAME} PRIVATE
  ${EXAMPLES_SUPPORT_PATH}
)

musin_init(${EXECUTABLE_NAME})

pico_enable_stdio_usb(${EXECUTABLE_NAME} 0)
pico_enable_stdio_uart(${EXECUTABLE_NAME} 1)

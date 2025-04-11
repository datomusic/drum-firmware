cmake_minimum_required(VERSION 3.13...3.27)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PICO_BOARD dato_submarine)

set(EXECUTABLE_NAME midi_cc)

include(${CMAKE_CURRENT_LIST_DIR}/../../musin/musin_init.cmake)

project(${EXECUTABLE_NAME})

add_executable(${EXECUTABLE_NAME}
  main.cpp
)

target_compile_definitions(${EXECUTABLE_NAME} PRIVATE
  PICO_STDIO_USB_ENABLE_RESET_VIA_VENDOR_INTERFACE=1
)

musin_init(${EXECUTABLE_NAME})
musin_init_hal(${EXECUTABLE_NAME})

pico_enable_stdio_uart(${EXECUTABLE_NAME} 1)
pico_enable_stdio_usb(${EXECUTABLE_NAME} 1)
pico_add_extra_outputs(${EXECUTABLE_NAME})
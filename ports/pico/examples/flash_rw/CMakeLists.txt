cmake_minimum_required(VERSION 3.13...3.27)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(EXECUTABLE_NAME flash_rw_demo)

set(SDK_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../pico-sdk/)
set(CORE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../core)

# initialize pico-sdk from submodule
# note: this must happen before project()
include(${SDK_PATH}/pico_sdk_init.cmake)

project(${EXECUTABLE_NAME})
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)


pico_sdk_init()
add_subdirectory(../../libraries/pico-vfs build)

add_executable(${EXECUTABLE_NAME}
    main.cpp
    ${CORE_PATH}/filesystem.c
)

target_link_libraries(${EXECUTABLE_NAME} PRIVATE
  pico_stdlib
  filesystem_vfs
)

target_include_directories(${EXECUTABLE_NAME} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

pico_enable_stdio_usb(${EXECUTABLE_NAME} 1)
pico_enable_stdio_uart(${EXECUTABLE_NAME} 0)
pico_add_extra_outputs(${EXECUTABLE_NAME})
pico_enable_filesystem(${EXECUTABLE_NAME})

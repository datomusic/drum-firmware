cmake_minimum_required(VERSION 3.13...3.27)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(EXECUTABLE_NAME midi_example)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../musin/musin_init.cmake)

project(${EXECUTABLE_NAME})

add_executable(${EXECUTABLE_NAME}
    main.cpp
)

musin_init(${EXECUTABLE_NAME})
musin_init_usb_midi(${EXECUTABLE_NAME})

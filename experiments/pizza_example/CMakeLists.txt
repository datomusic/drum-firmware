# CMakeLists.txt for the Drum Pizza example

cmake_minimum_required(VERSION 3.13...3.27)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(EXECUTABLE_NAME pizza_example)

# Include the common Musin CMake initialization script
# This handles SDK inclusion, project setup, standard options, etc.
include(${CMAKE_CURRENT_LIST_DIR}/../../musin/musin_init.cmake)

project(${EXECUTABLE_NAME})

# Define the executable target with its main source file
add_executable(${EXECUTABLE_NAME}
  main.cpp
  # Add board source file directly
  ${CMAKE_CURRENT_LIST_DIR}/../../musin/boards/drum_pizza.cpp
)

musin_init(${EXECUTABLE_NAME})
musin_init_drivers(${EXECUTABLE_NAME})
musin_init_hal(${EXECUTABLE_NAME})
musin_init_ui(${EXECUTABLE_NAME})

# Board sources are added directly above
# musin_init_boards(${EXECUTABLE_NAME})

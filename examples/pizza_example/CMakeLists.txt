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
)

# Initialize base Musin library dependencies (includes, sources, links, stdio, outputs)
musin_init(${EXECUTABLE_NAME})

# Initialize Musin UI library dependencies
# Handles keypad_hc138.cpp, ws2812.cpp, ws2812.pio generation, and linking gpio, pio, clocks
musin_init_ui(${EXECUTABLE_NAME})

# Initialize Musin board support dependencies
# Should handle drum_pizza.cpp (assuming musin_init_boards macro exists/will exist)
musin_init_boards(${EXECUTABLE_NAME})

# Note: stdio setup (USB/UART) and extra outputs (uf2) are handled by musin_init()

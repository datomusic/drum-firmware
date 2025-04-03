# CMakeLists.txt for the Drum Pizza example

cmake_minimum_required(VERSION 3.13)

# Pull in SDK (must be before project)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(pizza_example C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the SDK
pico_sdk_init()

# --- PIO Programs ---
# Add the PIO file relative to this example's CMakeLists.txt
# Assuming the musin library structure is accessible relative to this example
pico_generate_pio_header(pizza_example_pio ${CMAKE_CURRENT_LIST_DIR}/../../musin/ui/ws2812.pio)

# --- Executable ---
add_executable(pizza_example)

# Add source files
target_sources(pizza_example PRIVATE
    main.cpp
    # Add required Musin library source files
    ${CMAKE_CURRENT_LIST_DIR}/../../musin/boards/drum_pizza.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../musin/ui/keypad_hc138.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../musin/ui/ws2812.cpp
)

# Add include directories (to find musin headers)
target_include_directories(pizza_example PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../..
)

# Link libraries
target_link_libraries(pizza_example PRIVATE
    pico_stdlib
    hardware_pio      # Needed for WS2812
    hardware_clocks   # Needed for WS2812 clock calculation
    hardware_gpio     # Needed by keypad_hc138 and drum_pizza init checks
)

# Ensure the PIO header generation happens before compiling the executable
# Use the target name defined in pico_generate_pio_header
add_dependencies(pizza_example pizza_example_pio)

# Add USB Mass Storage Device support for flashing (optional)
pico_enable_stdio_usb(pizza_example 1)
pico_enable_stdio_uart(pizza_example 1)

# Create map/bin/hex file etc.
pico_add_extra_outputs(pizza_example)

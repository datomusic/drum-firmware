cmake_minimum_required(VERSION 3.12)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(ws2812_blink C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# --- PIO Programs ---
# Generate the header for the WS2812 PIO program
# The target name here (ws2812_pio_lib) doesn't really matter, it's just
# used internally by CMake to track the generation step.
# The important part is that it generates ws2812.pio.h in the build directory.
pico_generate_pio_header(ws2812_pio_lib ${CMAKE_CURRENT_LIST_DIR}/musin/ui/ws2812.pio)


# --- Executable ---
add_executable(ws2812_blink)

# Add source files
target_sources(ws2812_blink PRIVATE
    main.cpp
    musin/ui/ws2812.cpp
    # Add other source files like drum_pizza.cpp, keypad_hc138.cpp etc. here
    # Example:
    musin/boards/drum_pizza.cpp
    musin/ui/keypad_hc138.cpp
)

# Add include directories (if needed, e.g., for musin base directory)
target_include_directories(ws2812_blink PRIVATE ${CMAKE_CURRENT_LIST_DIR})

# Link libraries
target_link_libraries(ws2812_blink PRIVATE
    pico_stdlib
    hardware_pio      # Needed for PIO functions
    hardware_clocks   # Needed for clock_get_hz
    # Add other hardware libraries if used (e.g., hardware_gpio, hardware_dma)
    hardware_gpio     # Needed by keypad_hc138 and drum_pizza init checks
)

# Ensure the PIO header generation happens before compiling the executable
add_dependencies(ws2812_blink ws2812_pio_lib)


pico_add_extra_outputs(ws2812_blink)

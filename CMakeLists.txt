cmake_minimum_required(VERSION 3.12)

# --- Pico SDK & Extras Path Setup ---
# Check if PICO_SDK_PATH is set, otherwise provide a default relative path
# Assumes SDK is located within musin/ports/pico/ relative to project root
if (NOT DEFINED ENV{PICO_SDK_PATH} AND NOT DEFINED PICO_SDK_PATH)
    set(PICO_SDK_PATH ${CMAKE_CURRENT_SOURCE_DIR}/musin/ports/pico/pico-sdk CACHE PATH "Path to Pico SDK")
    message(STATUS "PICO_SDK_PATH not set, defaulting to ${PICO_SDK_PATH}")
elseif(DEFINED ENV{PICO_SDK_PATH} AND NOT DEFINED PICO_SDK_PATH)
    set(PICO_SDK_PATH $ENV{PICO_SDK_PATH} CACHE PATH "Path to Pico SDK")
    message(STATUS "PICO_SDK_PATH taken from environment: ${PICO_SDK_PATH}")
endif()

if (NOT EXISTS ${PICO_SDK_PATH})
    message(FATAL_ERROR "Pico SDK path not found: ${PICO_SDK_PATH}. Please set PICO_SDK_PATH environment variable or ensure the default path is correct.")
endif()

# Assume pico-extras is a sibling directory to pico-sdk
get_filename_component(PICO_SDK_PARENT_DIR ${PICO_SDK_PATH} DIRECTORY)
set(PICO_EXTRAS_PATH ${PICO_SDK_PARENT_DIR}/pico-extras CACHE PATH "Path to Pico Extras")
if (NOT EXISTS ${PICO_EXTRAS_PATH})
    message(WARNING "Pico Extras path not found: ${PICO_EXTRAS_PATH}. Some features might be unavailable.")
endif()

# Include the SDK import script BEFORE project()
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(ws2812_blink C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the SDK AFTER project()
pico_sdk_init()

# Include extras import script AFTER pico_sdk_init()
if (EXISTS ${PICO_EXTRAS_PATH}/external/pico_extras_import.cmake)
    include(${PICO_EXTRAS_PATH}/external/pico_extras_import.cmake)
endif()


# --- PIO Programs ---
# PIO headers are generated per-target using pico_generate_pio_header below.


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

# Generate PIO header for this target
pico_generate_pio_header(ws2812_blink ${CMAKE_CURRENT_LIST_DIR}/musin/drivers/ws2812.pio) # Corrected path to drivers dir

pico_add_extra_outputs(ws2812_blink)


# ====================================================================
# Drum Pad Test Experiment
# ====================================================================
add_executable(drum_pad_test)

# Add source files for the experiment and the required Musin components
target_sources(drum_pad_test PRIVATE
    experiments/drum_pad_test/main.cpp
    musin/boards/drum_pizza.cpp
    musin/hal/analog_in.cpp
    musin/ui/keypad_hc138.cpp
    musin/drivers/ws2812.cpp
    # musin/ui/drumpad.cpp # Assuming drumpad is template-only in header
)

# Add include directories
target_include_directories(drum_pad_test PRIVATE ${CMAKE_CURRENT_LIST_DIR})

# Link libraries required by the experiment and its dependencies
target_link_libraries(drum_pad_test PRIVATE
    pico_stdlib
    hardware_adc      # Needed by analog_in
    hardware_gpio     # Needed by analog_in, keypad, drum_pizza init checks
    hardware_pio      # Needed by ws2812
    hardware_clocks   # Needed by ws2812
    # hardware_dma    # Add if ws2812 uses DMA
)

# Generate PIO header for this target
pico_generate_pio_header(drum_pad_test ${CMAKE_CURRENT_LIST_DIR}/musin/drivers/ws2812.pio) # Corrected path to drivers dir

pico_add_extra_outputs(drum_pad_test)


# ====================================================================
# Pizza Example Experiment
# ====================================================================
add_subdirectory(experiments/pizza_example)

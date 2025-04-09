# CMakeLists.txt for the Drum Pizza example

cmake_minimum_required(VERSION 3.13...3.27)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(PICO_BOARD dato_submarine)
set(EXECUTABLE_NAME pizza_example)

project(${EXECUTABLE_NAME})
set(CMAKE_CXX_STANDARD 20) # Ensure CXX standard is set

# --- Musin Setup ---
# Add the Musin library definitions by including its directory
# This should happen BEFORE pico_sdk_init if Musin defines components
# that rely on SDK functions during configuration.
add_subdirectory(../../musin musin_build)

# --- Pico SDK Setup ---
# pico_sdk_init() initializes the SDK for the project.
# It uses PICO_SDK_PATH which should be set globally (by env or top-level CMakeLists.txt)
pico_sdk_init()

# Include extras import script AFTER pico_sdk_init()
# PICO_EXTRAS_PATH should also be set globally
if (EXISTS ${PICO_EXTRAS_PATH}/external/pico_extras_import.cmake)
    include(${PICO_EXTRAS_PATH}/external/pico_extras_import.cmake)
else()
    message(WARNING "Pico Extras import script not found at ${PICO_EXTRAS_PATH}/external/pico_extras_import.cmake")
endif()

# Include the remaining macros from the old init file (will be removed later)
# This needs PICO_EXTRAS_PATH to be set for warning suppression logic inside.
include(../../musin/musin_init.cmake)

# --- Executable Definition ---
add_executable(${EXECUTABLE_NAME}
  main.cpp
  # Add board source file directly (will likely become part of a musin::board_drum_pizza library)
  ${CMAKE_CURRENT_LIST_DIR}/../../musin/boards/drum_pizza.cpp
)

# --- Link Libraries ---
target_link_libraries(${EXECUTABLE_NAME} PRIVATE
    # Core Musin library (provides pico_uart, etl, pico_stdlib, hardware_uart)
    musin::core

    # Pico SDK hardware libraries (Keep direct links for now, will be dependencies of musin::* libs)
    hardware_adc      # For analog_in (will move to musin::hal)
    hardware_gpio     # For keypad_hc138, analog_in mux pins (will move to musin::ui/hal)
    hardware_pio      # For ws2812 (will move to musin::drivers)
    hardware_dma      # For ws2812 (potentially) (will move to musin::drivers)
    hardware_clocks   # For ws2812 timing (will move to musin::drivers)

    # Add other SDK libs if needed directly by main.cpp
)

# --- Use remaining Musin macros (will be replaced by linking musin::* libs) ---
# musin_init(${EXECUTABLE_NAME}) # Replaced by linking musin::core and explicit Pico setup
musin_init_drivers(${EXECUTABLE_NAME})
musin_init_hal(${EXECUTABLE_NAME})
musin_init_ui(${EXECUTABLE_NAME})

# --- Final Pico Setup for the Executable ---
pico_enable_stdio_uart(${EXECUTABLE_NAME} 1)
pico_enable_stdio_usb(${EXECUTABLE_NAME} 1)
pico_add_extra_outputs(${EXECUTABLE_NAME})
target_compile_options(${EXECUTABLE_NAME} PRIVATE -Wall -Wextra)

# --- Suppress Warnings (Keep for now, might move to specific Musin targets) ---
# set_source_files_properties(${SDK_EXTRAS_PATH}/src/rp2_common/pico_audio_i2s/audio_i2s.c PROPERTIES COMPILE_FLAGS -Wno-unused-parameter)
# set_source_files_properties(${SDK_EXTRAS_PATH}/src/common/pico_audio/audio.cpp PROPERTIES COMPILE_FLAGS -Wno-missing-field-initializers)

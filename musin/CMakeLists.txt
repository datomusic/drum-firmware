# Musin Project CMake Build File

cmake_minimum_required(VERSION 3.13) # INTERFACE libraries benefit from newer versions

# --- Basic Project Setup ---
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

set(MUSIN_ROOT ${CMAKE_CURRENT_LIST_DIR})
set(MUSIN_LIBRARIES ${MUSIN_ROOT}/ports/pico/libraries)
set(MUSIN_USB ${MUSIN_ROOT}/usb)
set(MUSIN_DRIVERS ${MUSIN_ROOT}/drivers)
set(MUSIN_AUDIO ${MUSIN_ROOT}/audio)
set(MUSIN_UI ${MUSIN_ROOT}/ui)
set(MUSIN_HAL ${MUSIN_ROOT}/hal)

# --- Pico SDK and Extras Setup ---
# Assume SDK_PATH and SDK_EXTRAS_PATH are set by the environment or a parent CMakeLists
# If not, uncomment and adjust these lines:
# set(SDK_PATH ${MUSIN_ROOT}/ports/pico/pico-sdk/)
# set(SDK_EXTRAS_PATH ${MUSIN_ROOT}/ports/pico/pico-extras/)

# Add custom board directory before SDK init
list(APPEND PICO_BOARD_HEADER_DIRS ${MUSIN_ROOT}/boards)

# Initialize pico-sdk from submodule
# Note: This might be better done in the top-level application CMakeLists
# if Musin is intended to be usable outside a Pico context eventually.
# For now, keep it here for simplicity with current examples.
# include(${SDK_PATH}/pico_sdk_init.cmake) # Assuming this is done by the application's pico_sdk_init()
# include(${SDK_EXTRAS_PATH}/external/pico_extras_import.cmake) # Assuming this is done by the application

# --- External Libraries ---
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../lib/etl etl_build)

# --- Musin Component Libraries ---

# musin::core
# Basic utilities, UART wrapper, common dependencies
add_library(musin::core STATIC
    ${MUSIN_ROOT}/pico_uart.cpp
)
target_include_directories(musin::core PUBLIC
    # Public include path allows applications to use #include "musin/pico_uart.h" etc.
    ${MUSIN_ROOT}/..
)
target_link_libraries(musin::core PUBLIC
    pico_stdlib     # Needed by pico_uart.cpp and commonly used
    hardware_uart   # Needed by pico_uart.cpp
    etl::etl        # Common dependency
)

# --- Add other component definitions below ---
# (e.g., musin::hal, musin::drivers, musin::ui, musin::audio, musin::usb_midi, musin::filesystem)
# These will be added in subsequent steps.

# --- Global Settings (Optional) ---
# Apply common warnings across all Musin components if desired
# target_compile_options(musin::core PUBLIC -Wall -Wextra) # Example

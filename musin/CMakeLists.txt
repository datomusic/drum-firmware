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
# Add custom board directory (this is okay to do here)
list(APPEND PICO_BOARD_HEADER_DIRS ${MUSIN_ROOT}/boards)


# --- Pico SDK and Toolchain Setup ---
# Define the SDK path *first*
set(PICO_SDK_PATH ${MUSIN_PATH}/ports/pico/pico-sdk CACHE PATH "Path to the Pico SDK")
set(PICO_SDK_EXTRAS_PATH ${MUSIN_PATH}/ports/pico/pico-extras/)

# Initialize pico-sdk from submodule
# Note: This might be better done in the top-level application CMakeLists
# if Musin is intended to be usable outside a Pico context eventually.

# Initialize the Pico SDK *after* the board and header dirs are set
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)
# Import targets from pico-extras (like pico_audio)
include(${PICO_SDK_EXTRAS_PATH}/external/pico_extras_import.cmake)


# Initialize the Pico SDK *after* the board and toolchain are set
pico_sdk_init()

# --- External Libraries ---
# Ensure ETL is added. If it depends on SDK types, it should also assume
# the SDK is initialized by the application.
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../lib/etl etl_build)

# --- Musin Component Libraries ---

# musin_core (Internal static library)
# Basic utilities, UART wrapper, common dependencies
add_library(musin_core STATIC
    ${MUSIN_ROOT}/pico_uart.cpp
)
target_include_directories(musin_core PUBLIC
    # Public include path allows applications to use #include "musin/pico_uart.h" etc.
    ${MUSIN_ROOT}/..
)
target_link_libraries(musin_core PUBLIC
    pico_stdlib     # Needed by pico_uart.cpp and commonly used
    hardware_uart   # Needed by pico_uart.cpp
    etl::etl        # Common dependency
)
# musin::core (Public alias)
add_library(musin::core ALIAS musin_core)

set_source_files_properties(${PICO_SDK_EXTRAS_PATH}/src/common/pico_audio/audio.cpp PROPERTIES COMPILE_FLAGS -Wno-missing-field-initializers)

# Enable stdio USB and UART by default for all Musin components
pico_enable_stdio_usb(musin_core 1)
pico_enable_stdio_uart(musin_core 1)

# --- musin::hal ---
# Internal static library containing the implementation
add_library(musin_hal_internal STATIC
  ${MUSIN_HAL}/analog_in.cpp
)
target_include_directories(musin_hal_internal PRIVATE
  ${MUSIN_HAL} # For internal headers if any
)
target_link_libraries(musin_hal_internal PRIVATE
  musin::core     # Depends on core utilities/types
  hardware_adc
  hardware_dma    # Often used with ADC
  hardware_gpio   # For pin configuration
  # hardware_pio    # Uncomment if any HAL component uses PIO directly
)

# Public interface library (simple name)
add_library(musin_hal_interface INTERFACE)
target_include_directories(musin_hal_interface INTERFACE
  ${MUSIN_ROOT}/.. # Expose includes like "musin/hal/analog_in.h"
)
target_link_libraries(musin_hal_interface INTERFACE
  musin_hal_internal # Link the implementation
  # Propagate public dependencies to users
  musin::core
  hardware_adc
  hardware_gpio
  # hardware_dma # Only propagate if needed by the public interface/headers
)
# Public alias with '::' syntax
add_library(musin::hal ALIAS musin_hal_interface)


# --- musin::drivers ---
# Internal static library
add_library(musin_drivers_internal STATIC
  ${MUSIN_DRIVERS}/ws2812.cpp
  ${MUSIN_DRIVERS}/aic3204.c # Note: This is C code
)
target_include_directories(musin_drivers_internal PRIVATE
  ${MUSIN_DRIVERS}
)
# Generate the PIO header required by ws2812.cpp and add its include path
# to the musin_drivers_internal target.
pico_generate_pio_header(musin_drivers_internal ${MUSIN_DRIVERS}/ws2812.pio)

target_link_libraries(musin_drivers_internal PRIVATE
  musin::core
  hardware_pio # For ws2812
  hardware_dma # For ws2812 / PIO interaction
  hardware_i2c # For aic3204
  hardware_clocks # For ws2812 PIO init helper
)

# Public interface library (simple name)
add_library(musin_drivers_interface INTERFACE)
target_include_directories(musin_drivers_interface INTERFACE
  ${MUSIN_ROOT}/.. # Expose includes like "musin/drivers/ws2812.h"
)
target_link_libraries(musin_drivers_interface INTERFACE
  musin_drivers_internal
  # Propagate public dependencies
  musin::core
  hardware_pio
  hardware_i2c
  hardware_clocks
)
# Public alias with '::' syntax
add_library(musin::drivers ALIAS musin_drivers_interface)


# --- musin::ui ---
# Internal static library
add_library(musin_ui_internal STATIC
  ${MUSIN_UI}/keypad_hc138.cpp
  # Add other UI source files here (e.g., drumpad.cpp if it exists)
)
target_include_directories(musin_ui_internal PRIVATE
  ${MUSIN_UI}
)
target_link_libraries(musin_ui_internal PRIVATE
  musin::core
  hardware_gpio
  pico_stdlib # For time functions used in keypad_hc138
)

# Public interface library (simple name)
add_library(musin_ui_interface INTERFACE)
target_include_directories(musin_ui_interface INTERFACE
  ${MUSIN_ROOT}/.. # Expose includes like "musin/ui/keypad_hc138.h"
)
target_link_libraries(musin_ui_interface INTERFACE
  musin_ui_internal
  # Propagate public dependencies
  musin::core
  hardware_gpio
  pico_stdlib
)
# Public alias with '::' syntax
add_library(musin::ui ALIAS musin_ui_interface)


# --- musin::audio ---
# Internal static library
add_library(musin_audio_internal STATIC
  ${MUSIN_AUDIO}/audio_output.cpp
  ${MUSIN_AUDIO}/pitch_shifter.cpp
  ${MUSIN_AUDIO}/audio_memory_reader.cpp
  ${MUSIN_AUDIO}/data_ulaw.c
  ${MUSIN_AUDIO}/mixer.cpp
  ${MUSIN_AUDIO}/crusher.cpp
  ${MUSIN_AUDIO}/waveshaper.cpp
  ${MUSIN_AUDIO}/filter.cpp
)
target_include_directories(musin_audio_internal PRIVATE
  ${MUSIN_AUDIO}
)
target_compile_definitions(musin_audio_internal PRIVATE
  PICO_AUDIO_I2S_MONO_INPUT=1
  USE_AUDIO_I2S=1
  AUDIO_BLOCK_SAMPLES=32  # Define the missing symbol
)
target_link_libraries(musin_audio_internal PRIVATE
  musin::core
  musin::drivers # Depends on the codec driver (aic3204)
  hardware_dma
  hardware_pio   # For pico_audio_i2s
  hardware_i2c   # Needed by aic3204 via musin::drivers
  hardware_irq
  pico_audio_i2s
)

# Public interface library (simple name)
add_library(musin_audio_interface INTERFACE)
target_include_directories(musin_audio_interface INTERFACE
  ${MUSIN_ROOT}/.. # Expose includes like "musin/audio/mixer.h"
)
target_link_libraries(musin_audio_interface INTERFACE
  musin_audio_internal
  # Propagate public dependencies
  musin::core
  musin::drivers # Expose driver dependency (e.g., for codec control)
  pico_audio_i2s
  etl::etl       # Audio headers use etl::array
)
# Public alias with '::' syntax
add_library(musin::audio ALIAS musin_audio_interface)


# --- musin::usb_midi ---
# Requires TinyUSB to be set up by the application's pico_sdk_init()
# Internal static library
add_library(musin_usb_midi_internal STATIC
  ${MUSIN_USB}/usb.cpp
  ${MUSIN_USB}/usb_descriptors.c # Note: This is C code
  ${MUSIN_USB}/midi_usb_bridge/MIDIUSB.cpp
  ${MUSIN_ROOT}/midi/midi_wrapper.cpp
)
target_include_directories(musin_usb_midi_internal PRIVATE
  ${MUSIN_USB}
  ${MUSIN_USB}/midi_usb_bridge
  ${MUSIN_LIBRARIES}/arduino_midi_library/src # Assumes this path is correct
  ${MUSIN_LIBRARIES}/Arduino-USBMIDI/src     # Needed for USB-MIDI.h
  ${MUSIN_ROOT}/midi # For midi_wrapper.h
)
# This definition often needs to be applied to the *final executable*
# for TinyUSB stack configuration. Document this requirement for users.
target_compile_definitions(musin_usb_midi_internal PRIVATE
   PICO_STDIO_USB_ENABLE_RESET_VIA_VENDOR_INTERFACE=1
)
target_link_libraries(musin_usb_midi_internal PRIVATE
  musin::core
  pico_stdio_usb # Needed for tusb_config.h -> pico/stdio_usb.h
  tinyusb_device
  tinyusb_board
  # pico_stdlib # Should be linked by tinyusb or musin::core
)

# Public interface library (simple name)
add_library(musin_usb_midi_interface INTERFACE)
target_include_directories(musin_usb_midi_interface INTERFACE
  ${MUSIN_ROOT}/.. # Expose includes like "musin/usb/usb.h", "musin/midi/midi_wrapper.h"
  ${MUSIN_LIBRARIES}/arduino_midi_library/src # Expose MIDI library headers (Defs.h etc)
  ${MUSIN_LIBRARIES}/Arduino-USBMIDI/src     # Expose USB-MIDI headers if needed
)
target_link_libraries(musin_usb_midi_interface INTERFACE
  musin_usb_midi_internal
  # Propagate public dependencies
  musin::core
  tinyusb_device
  tinyusb_board
)
# Public alias with '::' syntax
add_library(musin::usb_midi ALIAS musin_usb_midi_interface)
# Add compile definitions publicly if they affect the interface or usage
# target_compile_definitions(musin_usb_midi_interface INTERFACE
#   SOME_PUBLIC_DEFINE=1
# )


# --- musin::filesystem ---
# Requires pico-vfs to be added via add_subdirectory in the application
# or potentially here if Musin always bundles it.
# Assuming pico-vfs is added elsewhere (e.g., by the application or a top-level CMakeLists)
# and provides the 'filesystem_vfs' target.
# If Musin should *always* include pico-vfs, uncomment the add_subdirectory line.
add_subdirectory(${MUSIN_ROOT}/ports/pico/libraries/pico-vfs vfs_build)

# Internal static library
add_library(musin_filesystem_internal STATIC
  ${MUSIN_ROOT}/filesystem/filesystem.cpp
)
target_include_directories(musin_filesystem_internal PRIVATE
  ${MUSIN_ROOT}/filesystem
  # Rely on filesystem_vfs target to provide necessary include paths (like for lfs.h)
)
target_link_libraries(musin_filesystem_internal PRIVATE
  musin::core
  filesystem_vfs      # Provided by pico-vfs (for VFS API)
  filesystem_littlefs # Provided by pico-vfs (for lfs.h include path)
  hardware_flash      # Needed for flash operations within filesystem.cpp
)

# Public interface library (simple name)
add_library(musin_filesystem_interface INTERFACE)
target_include_directories(musin_filesystem_interface INTERFACE
  ${MUSIN_ROOT}/.. # Expose includes like "musin/filesystem/filesystem.h"
)
target_link_libraries(musin_filesystem_interface INTERFACE
  musin_filesystem_internal
  # Propagate public dependencies
  musin::core
  filesystem_vfs
)
# Public alias with '::' syntax
add_library(musin::filesystem ALIAS musin_filesystem_interface)
# IMPORTANT: The application linking against musin::filesystem *must* call
# pico_enable_filesystem(your_app_target) in its own CMakeLists.txt


# --- Global Settings (Optional) ---
# Apply common warnings across all Musin components if desired
# find_package(Warnings) # Example using a hypothetical Warnings package
# set(MUSIN_WARNINGS -Wall -Wextra -Wpedantic)
# target_compile_options(musin_core PRIVATE ${MUSIN_WARNINGS})
# target_compile_options(musin_hal_internal PRIVATE ${MUSIN_WARNINGS})
# ... etc for all internal static libs

cmake_minimum_required(VERSION 3.13...3.27)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enable support for RP2350 A2 chip erratum E10. This ensures the
# --abs-block flag is added to UF2 files for compatibility.
set(PICO_RP2350_A2_SUPPORTED ON)

set(PICO_BOARD dato_submarine)

set(EXECUTABLE_NAME_BASE drum)

include(${CMAKE_CURRENT_LIST_DIR}/../musin/musin.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/../musin/version.cmake)

# Configure version variables first
configure_version_from_git()

set(EXECUTABLE_NAME ${EXECUTABLE_NAME_BASE}-${VERSION_FILENAME})

# Generate version header file
generate_version_header(${CMAKE_BINARY_DIR}/generated/version.h)

project(${EXECUTABLE_NAME})

add_executable(${EXECUTABLE_NAME}
  main.cpp
  test_midi_manager.cpp
  usb_descriptors.c
  ../musin/midi/midi_sender.cpp
  # Test framework
  test_framework/test_manager.cpp
  command/command_parser.cpp
  command/response_formatter.cpp
  # Tests
  tests/midi/midi_loopback_test.cpp
  tests/midi/midi_thru_test.cpp
  tests/sync/sync_loopback_test.cpp
)

target_compile_features(${EXECUTABLE_NAME} PRIVATE cxx_std_20)

# Factory kit generation disabled for test machine build

target_compile_definitions(${EXECUTABLE_NAME} PRIVATE
  USBD_MANUFACTURER="Dato"
  USBD_PRODUCT="Dato DRUM Testmachine"
  PICO_PROGRAM_DESCRIPTION="DRUM Testmachine"
  PICO_PROGRAM_VERSION_STRING="${VERSION_STRING}"
  SWAP_AUDIO_CLOCK=1
  PICO_STDIO_USB_RESET_INTERFACE_SUPPORT_MS_OS_20_DESCRIPTOR=0
)

if (SWAP_AUDIO_CLOCK)
  target_compile_definitions(${EXECUTABLE_NAME} PRIVATE PICO_AUDIO_I2S_CLOCK_PINS_SWAPPED=1)
endif(SWAP_AUDIO_CLOCK)

# Add include directories for the target
target_include_directories(${EXECUTABLE_NAME} PRIVATE
  ${CMAKE_BINARY_DIR}/generated # For version.h and config_default.h
  ${CMAKE_CURRENT_SOURCE_DIR}/config
  # Add other specific include paths here if needed
  .
  ./lib
)

# To enable verbose logging, pass -DENABLE_VERBOSE_LOGGING=ON to the cmake command.
if (NOT DEFINED ENABLE_VERBOSE_LOGGING)
  set(ENABLE_VERBOSE_LOGGING OFF CACHE BOOL "Enable verbose logging")
endif()

if (ENABLE_VERBOSE_LOGGING)
  message(STATUS "Verbose logging enabled.")
  target_compile_definitions(${EXECUTABLE_NAME} PRIVATE VERBOSE)
endif()

pico_enable_stdio_usb(${EXECUTABLE_NAME} 1)

# --- Setup musin INTERFACE libraries ---
# Order matters: setup libraries with fewer dependencies first.
musin_setup_hal_target()
musin_setup_drivers_target()
musin_setup_ui_target()
musin_setup_usb_midi_target()

# Core depends on hal and usb_midi, so it must be last.
musin_setup_core_target()

# --- Link application against musin libraries ---
target_link_libraries(${EXECUTABLE_NAME} PRIVATE
  musin::core
  musin::hal
  musin::drivers
  musin::ui
  musin::usb_midi
)

# Explicitly find the size utility, hinting at the Pico toolchain path
set(TOOLCHAIN_PREFIX "arm-none-eabi-") # Adjust if your toolchain prefix is different
find_program(SIZE_UTIL_PATH NAMES ${TOOLCHAIN_PREFIX}size HINTS $ENV{PICO_TOOLCHAIN_PATH}) # Tell CMake where to look
if (NOT SIZE_UTIL_PATH)
  message(WARNING "Size utility (${TOOLCHAIN_PREFIX}size) not found in standard paths or ${PICO_TOOLCHAIN_PATH}. Post-build size report will be skipped.")
endif()

# Add size reporting commands only if the utility was found
if (SIZE_UTIL_PATH)
  add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD
    COMMAND ${SIZE_UTIL_PATH} -A $<TARGET_FILE:${EXECUTABLE_NAME}>
    COMMENT "Detailed size report:"
  )

  add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD
    COMMAND ${SIZE_UTIL_PATH} $<TARGET_FILE:${EXECUTABLE_NAME}>
    COMMENT "Concise size report:"
  )
endif()

pico_add_extra_outputs(${EXECUTABLE_NAME})

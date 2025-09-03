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
  midi_manager.cpp
  sysex_handler.cpp
  pizza_controls.cpp
  ui/pizza_display.cpp
  sequencer_controller.cpp
  configuration_manager.cpp
  message_router.cpp
  audio_engine.cpp
  sample_repository.cpp
  ui/display_mode.cpp
  system_state_machine.cpp
  state_implementations.cpp
  usb_descriptors.c
  ../musin/midi/midi_sender.cpp
  drumpad_factory.cpp
)

target_compile_features(${EXECUTABLE_NAME} PRIVATE cxx_std_20)

# --- Generate binary kit file from C++ source ---
set(KIT_NAME factory_kit)
set(KIT_SRC_CPP "${CMAKE_CURRENT_SOURCE_DIR}/${KIT_NAME}.cpp")
set(KIT_BINARY_FILE "${CMAKE_CURRENT_SOURCE_DIR}/${KIT_NAME}.bin")

# 1. Add a dummy library that ONLY contains our kit source file.
# The pico toolchain setup will automatically apply the correct compiler flags.
add_library(${KIT_NAME}_obj OBJECT ${KIT_SRC_CPP})
target_link_libraries(${KIT_NAME}_obj PRIVATE etl::etl)

# 2. Find objcopy
find_program(OBJCOPY_PATH NAMES arm-none-eabi-objcopy HINTS ENV PICO_TOOLCHAIN_PATH)
if(NOT OBJCOPY_PATH)
    message(FATAL_ERROR "arm-none-eabi-objcopy not found. Make sure the PICO_TOOLCHAIN_PATH is set.")
endif()

# 3. Add a custom command to extract the data from the compiled object file.
# The object file is found in a specific location within the build directory.
set(KIT_OBJECT_FILE "$<TARGET_OBJECTS:${KIT_NAME}_obj>")

add_custom_command(
    OUTPUT ${KIT_BINARY_FILE}
    COMMAND ${OBJCOPY_PATH} -O binary -j .rodata ${KIT_OBJECT_FILE} ${KIT_BINARY_FILE}
    DEPENDS ${KIT_NAME}_obj
    COMMENT "Generating ${KIT_BINARY_FILE} from ${KIT_SRC_CPP}"
    VERBATIM
)

# 4. Add a target to drive the generation.
add_custom_target(generate_kit_binaries DEPENDS ${KIT_BINARY_FILE})


target_compile_definitions(${EXECUTABLE_NAME} PRIVATE
  USBD_MANUFACTURER="Dato"
  USBD_PRODUCT="DRUM"
  PICO_PROGRAM_DESCRIPTION="DRUM"
  PICO_PROGRAM_VERSION_STRING="${VERSION_STRING}"
  SWAP_AUDIO_CLOCK=1
)

if(SWAP_AUDIO_CLOCK)
  target_compile_definitions(${EXECUTABLE_NAME} PRIVATE
    PICO_AUDIO_I2S_CLOCK_PINS_SWAPPED=1
  )
endif(SWAP_AUDIO_CLOCK)

# Add include directories for the target
target_include_directories(${EXECUTABLE_NAME} PRIVATE
    ${CMAKE_BINARY_DIR}/generated # For version.h and config_default.h
    ${CMAKE_CURRENT_SOURCE_DIR}/config
    # Add other specific include paths here if needed
    ../lib
)

# To enable verbose logging, pass -DENABLE_VERBOSE_LOGGING=ON to the cmake command.
if(NOT DEFINED ENABLE_VERBOSE_LOGGING)
  set(ENABLE_VERBOSE_LOGGING OFF CACHE BOOL "Enable verbose logging")
endif()

if(ENABLE_VERBOSE_LOGGING)
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
musin_setup_audio_target()
musin_setup_filesystem_target()

# Core depends on hal and usb_midi, so it must be last.
musin_setup_core_target()

# --- Link application against musin libraries ---
target_link_libraries(${EXECUTABLE_NAME} PRIVATE
    musin::core
    musin::hal
    musin::drivers
    musin::ui
    musin::usb_midi
    musin::audio
    musin::filesystem
)

# Enable filesystem for the pico (must be done after linking)
pico_enable_filesystem(${EXECUTABLE_NAME})

add_dependencies(${EXECUTABLE_NAME} generate_kit_binaries)


# Explicitly find the size utility, hinting at the Pico toolchain path
set(TOOLCHAIN_PREFIX "arm-none-eabi-") # Adjust if your toolchain prefix is different
find_program(SIZE_UTIL_PATH NAMES ${TOOLCHAIN_PREFIX}size
    HINTS $ENV{PICO_TOOLCHAIN_PATH} # Tell CMake where to look
)
if(NOT SIZE_UTIL_PATH)
    message(WARNING "Size utility (${TOOLCHAIN_PREFIX}size) not found in standard paths or ${PICO_TOOLCHAIN_PATH}. Post-build size report will be skipped.")
endif()


# Add size reporting commands only if the utility was found
if(SIZE_UTIL_PATH)
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

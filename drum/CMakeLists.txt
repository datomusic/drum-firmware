cmake_minimum_required(VERSION 3.13...3.27)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PICO_BOARD dato_submarine)

set(EXECUTABLE_NAME_BASE drum)

include(${CMAKE_CURRENT_LIST_DIR}/../musin/musin_init.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/../musin/version.cmake)

# Configure version variables first
configure_version_from_git()

set(EXECUTABLE_NAME ${EXECUTABLE_NAME_BASE}-${VERSION_FILENAME})

# Generate version header file
generate_version_header(${CMAKE_BINARY_DIR}/generated/version.h)


project(${EXECUTABLE_NAME})

add_executable(${EXECUTABLE_NAME}
  main.cpp
  midi_functions.cpp
  pizza_controls.cpp
  ui/pizza_display.cpp
  ../musin/timing/tempo_handler.cpp
  sequencer_controller.cpp
  configuration_manager.cpp
  sound_router.cpp
  audio_engine.cpp
  sample_repository.cpp
)

# Generate C header from default config JSON
set(DEFAULT_CONFIG_SRC "${CMAKE_CURRENT_SOURCE_DIR}/config.default.json")
set(DEFAULT_CONFIG_HDR "${CMAKE_BINARY_DIR}/generated/config_default.h")

find_program(XXD_PROGRAM xxd)
if(NOT XXD_PROGRAM)
    message(FATAL_ERROR "xxd not found! It is required to generate the default config header. Install it via system package manager (e.g., brew install xxd, or it may be part of vim-common).")
endif()

add_custom_command(
    OUTPUT ${DEFAULT_CONFIG_HDR}
    COMMAND bash -c "cd ${CMAKE_CURRENT_SOURCE_DIR} && ${XXD_PROGRAM} -i config.default.json > ${DEFAULT_CONFIG_HDR}"
    DEPENDS ${DEFAULT_CONFIG_SRC}
    COMMENT "Generating C header from default config"
    VERBATIM
)

add_custom_target(generate_default_config DEPENDS ${DEFAULT_CONFIG_HDR})

target_compile_definitions(${EXECUTABLE_NAME} PRIVATE
  PICO_STDIO_USB_ENABLE_RESET_VIA_VENDOR_INTERFACE=1
  USBD_MANUFACTURER="Dato"
  USBD_PRODUCT="DRUM"
  PICO_PROGRAM_DESCRIPTION="DRUM"
  PICO_PROGRAM_VERSION_STRING="${VERSION_STRING}"
  AUDIO_BLOCK_SAMPLES=128
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

musin_setup_core_target()
target_link_libraries(${EXECUTABLE_NAME} PRIVATE musin::core)
musin_setup_hal_target()
target_link_libraries(${EXECUTABLE_NAME} PRIVATE musin::hal)
musin_setup_drivers_target()
target_link_libraries(${EXECUTABLE_NAME} PRIVATE musin::drivers)
musin_setup_ui_target()
target_link_libraries(${EXECUTABLE_NAME} PRIVATE musin::ui)
musin_setup_usb_midi_target()
target_link_libraries(${EXECUTABLE_NAME} PRIVATE musin::usb_midi)
musin_init_audio(${EXECUTABLE_NAME})

# Setup and link against the filesystem library and enable it for the pico
musin_setup_filesystem_target()
target_link_libraries(${EXECUTABLE_NAME} PRIVATE musin::filesystem)
pico_enable_filesystem(${EXECUTABLE_NAME})

add_dependencies(${EXECUTABLE_NAME} generate_default_config)


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

# pico_set_program_url("https://dato.mu/drum")

### Rompler application
# add_subdirectory(applications/rompler)

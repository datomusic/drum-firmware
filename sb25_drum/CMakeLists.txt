cmake_minimum_required(VERSION 3.13...3.27)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PICO_BOARD dato_submarine)

set(EXECUTABLE_NAME sb25_drum)

include(${CMAKE_CURRENT_LIST_DIR}/../musin/musin_init.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/../musin/version.cmake)

# Generate version header file
generate_version_header(${CMAKE_BINARY_DIR}/generated/version.h)

# Add the generated directory to include paths
include_directories(${CMAKE_BINARY_DIR}/generated)

project(${EXECUTABLE_NAME})

add_executable(${EXECUTABLE_NAME}
  main.cpp
  midi_functions.cpp
  pizza_controls.cpp
  pizza_display.cpp
  ../musin/timing/tempo_handler.cpp
  ../musin/timing/tempo_multiplier.cpp
  sequencer_controller.cpp
  sound_router.cpp
  audio_engine.cpp
)

target_compile_definitions(${EXECUTABLE_NAME} PRIVATE
  PICO_STDIO_USB_ENABLE_RESET_VIA_VENDOR_INTERFACE=1
  USBD_MANUFACTURER="Dato"
  USBD_PRODUCT="DRUM"
  PICO_PROGRAM_DESCRIPTION="SB25 DRUM proto"
  PICO_PROGRAM_VERSION_STRING="${VERSION_STRING}"
  AUDIO_BLOCK_SAMPLES=128
)

# Add include directories for the target
target_include_directories(${EXECUTABLE_NAME} PRIVATE
    ../experiments/support # For all_samples.h
    # Add other specific include paths here if needed
)

musin_init(${EXECUTABLE_NAME})
musin_init_drivers(${EXECUTABLE_NAME})
musin_init_ui(${EXECUTABLE_NAME})
musin_init_hal(${EXECUTABLE_NAME})
musin_init_usb_midi(${EXECUTABLE_NAME})
musin_init_audio(${EXECUTABLE_NAME})


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

pico_enable_stdio_usb(${EXECUTABLE_NAME} 1)
pico_add_extra_outputs(${EXECUTABLE_NAME})

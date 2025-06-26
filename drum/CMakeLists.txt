cmake_minimum_required(VERSION 3.13...3.27)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PICO_BOARD dato_submarine)

set(EXECUTABLE_NAME drum)

include(${CMAKE_CURRENT_LIST_DIR}/../musin/musin_init.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/../musin/version.cmake)

# Configure version variables first
configure_version_from_git()

# Generate version header file
generate_version_header(${CMAKE_BINARY_DIR}/generated/version.h)

# Sample generation setup
find_program(XXD_PROGRAM xxd)
find_program(FFMPEG_PROGRAM ffmpeg)
if(NOT XXD_PROGRAM OR NOT FFMPEG_PROGRAM)
    message(FATAL_ERROR "xxd/ffmpeg not found! Required for sample generation. Install via:\n  Ubuntu/Debian: sudo apt install xxd ffmpeg\n  MacOS: brew install xxd ffmpeg")
endif()

set(GENERATE_SAMPLES_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/../support/generate_sample_headers.sh")
set(SAMPLES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../support/samples")
set(GENERATED_SAMPLES_OUTPUT_DIR "${CMAKE_BINARY_DIR}/generated/samples") # Output directory for generated sample files

if(EXISTS "${GENERATE_SAMPLES_SCRIPT}" AND EXISTS "${SAMPLES_DIR}")
  file(GLOB_RECURSE SAMPLE_FILES "${SAMPLES_DIR}/*.wav")
  
  add_custom_command(
    OUTPUT ${GENERATED_SAMPLES_OUTPUT_DIR}/all_samples.h # Main output artifact
    COMMAND ${XXD_PROGRAM} --version # Optional: keep for logging
    COMMAND ${FFMPEG_PROGRAM} -version # Optional: keep for logging
    COMMAND bash ${GENERATE_SAMPLES_SCRIPT} -s ${SAMPLES_DIR} -o ${GENERATED_SAMPLES_OUTPUT_DIR}
    COMMENT "Generating sample headers using"
    DEPENDS 
      ${GENERATE_SAMPLES_SCRIPT}
      ${SAMPLE_FILES}
    VERBATIM
  )
  
  add_custom_target(generate_samples DEPENDS ${GENERATED_SAMPLES_OUTPUT_DIR}/all_samples.h)
else()
  message(WARNING "Sample generation script/samples missing - firmware might not build correctly")
endif()

# Add the generated directory to include paths
include_directories(${CMAKE_BINARY_DIR}/generated) # For version.h
# Note: GENERATED_SAMPLES_OUTPUT_DIR will be added to target_include_directories later

project(${EXECUTABLE_NAME})

add_executable(${EXECUTABLE_NAME}
  main.cpp
  midi_functions.cpp
  pizza_controls.cpp
  pizza_display.cpp
  ../musin/timing/tempo_handler.cpp
  sequencer_controller.cpp
  configuration_manager.cpp
  sound_router.cpp
  audio_engine.cpp
  sample_repository.cpp
)

# Embed default configuration
pico_add_binary_data(${EXECUTABLE_NAME}
    FILES
        config.default.json
)

target_compile_definitions(${EXECUTABLE_NAME} PRIVATE
  PICO_STDIO_USB_ENABLE_RESET_VIA_VENDOR_INTERFACE=1
  USBD_MANUFACTURER="Dato"
  USBD_PRODUCT="DRUM"
  PICO_PROGRAM_DESCRIPTION="DRUM"
  PICO_PROGRAM_VERSION_STRING="${VERSION_STRING}"
  AUDIO_BLOCK_SAMPLES=128
)

# Add include directories for the target
target_include_directories(${EXECUTABLE_NAME} PRIVATE
    ../experiments/support # For sample_conversion.h and potentially other support files
    ${GENERATED_SAMPLES_OUTPUT_DIR} # For all_samples.h, sample_data.h etc.
    # Add other specific include paths here if needed
    ../lib
)

musin_init(${EXECUTABLE_NAME})
musin_init_drivers(${EXECUTABLE_NAME})
musin_init_ui(${EXECUTABLE_NAME})
musin_init_hal(${EXECUTABLE_NAME})
musin_init_usb_midi(${EXECUTABLE_NAME})
musin_init_audio(${EXECUTABLE_NAME})
musin_init_filesystem(${PROJECT_NAME})

# Add sample generation dependency after executable is defined
if(TARGET generate_samples)
  add_dependencies(${EXECUTABLE_NAME} generate_samples)
endif()


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

if(SWAP_AUDIO_CLOCK)
  target_compile_definitions(${EXECUTABLE_NAME} PRIVATE
    PICO_AUDIO_I2S_CLOCK_PINS_SWAPPED=1
  )
endif(SWAP_AUDIO_CLOCK)

pico_enable_stdio_usb(${EXECUTABLE_NAME} 1)
pico_add_extra_outputs(${EXECUTABLE_NAME})


### Rompler application
# add_subdirectory(applications/rompler)


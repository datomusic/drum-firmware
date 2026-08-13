cmake_minimum_required(VERSION 3.13...3.27)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enable support for RP2350 A2 chip erratum E10. This ensures the
# --abs-block flag is added to UF2 files for compatibility.
set(PICO_RP2350_A2_SUPPORTED ON)

set(PICO_BOARD dato_submarine)

set(EXECUTABLE_NAME_BASE duo)

include(${CMAKE_CURRENT_LIST_DIR}/../musin/musin.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/../musin/version.cmake)

configure_version_from_git()

set(EXECUTABLE_NAME ${EXECUTABLE_NAME_BASE}-${VERSION_FILENAME})

generate_version_header(${CMAKE_BINARY_DIR}/generated/version.h)

project(${EXECUTABLE_NAME})

add_executable(${EXECUTABLE_NAME}
  main.cpp
  seq.cpp
  audio/effect_custom_envelope.cpp
  ui/duo_display.cpp
  usb_descriptors.c
  ../musin/midi/midi_sender.cpp
)

target_compile_features(${EXECUTABLE_NAME} PRIVATE cxx_std_20)

target_compile_definitions(${EXECUTABLE_NAME} PRIVATE
  USBD_MANUFACTURER="Dato"
  USBD_PRODUCT="Dato DUO"
  PICO_PROGRAM_DESCRIPTION="DUO"
  PICO_PROGRAM_VERSION_STRING="${VERSION_STRING}"
  SWAP_AUDIO_CLOCK=1
  PICO_STDIO_USB_RESET_INTERFACE_SUPPORT_MS_OS_20_DESCRIPTOR=0
)

pico_set_binary_version(${EXECUTABLE_NAME} MAJOR ${VERSION_MAJOR} MINOR ${VERSION_MINOR})

if (SWAP_AUDIO_CLOCK)
  target_compile_definitions(${EXECUTABLE_NAME} PRIVATE PICO_AUDIO_I2S_CLOCK_PINS_SWAPPED=1)
endif(SWAP_AUDIO_CLOCK)

target_include_directories(${EXECUTABLE_NAME} PRIVATE
  ${CMAKE_BINARY_DIR}/generated # For version.h
  .
  ..
)

pico_enable_stdio_usb(${EXECUTABLE_NAME} 1)

# XIP builds use the DRUM's linker script that copies .rodata to RAM so the
# interrupt-driven audio render never reads flash (audio ISR RAM residency,
# port plan §4.3). Milestone 1 never writes flash, so this is belt and braces.
if(NOT PICO_COPY_TO_RAM)
  pico_set_linker_script(${EXECUTABLE_NAME}
                         ${CMAKE_CURRENT_SOURCE_DIR}/../drum/memmap_xip_rodata_in_ram.ld)
endif()

# --- Setup musin INTERFACE libraries ---
# Order matters: setup libraries with fewer dependencies first.
musin_setup_hal_target()
musin_setup_drivers_target()
musin_setup_ui_target()
musin_setup_usb_midi_target()
musin_setup_audio_target()

# Core depends on hal and usb_midi, so it must be last.
musin_setup_core_target()

target_link_libraries(${EXECUTABLE_NAME} PRIVATE
  musin::core
  musin::hal
  musin::drivers
  musin::ui
  musin::usb_midi
  musin::audio
  pico_rand
)

pico_add_extra_outputs(${EXECUTABLE_NAME})

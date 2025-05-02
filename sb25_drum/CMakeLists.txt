cmake_minimum_required(VERSION 3.13...3.27)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PICO_BOARD dato_submarine)

set(EXECUTABLE_NAME sb25_drum)

include(${CMAKE_CURRENT_LIST_DIR}/add_git_defines.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/../musin/musin_init.cmake)

project(${EXECUTABLE_NAME})

add_executable(${EXECUTABLE_NAME}
  main.cpp
  midi.cpp
  pizza_controls.cpp
  pizza_display.cpp
  tempo_handler.cpp
  internal_clock.cpp
  tempo_multiplier.cpp
  sequencer_controller.cpp
  debug_utils.cpp
)

target_compile_definitions(${EXECUTABLE_NAME} PRIVATE
  PICO_STDIO_USB_ENABLE_RESET_VIA_VENDOR_INTERFACE=1
  USBD_MANUFACTURER="Dato"
  USBD_PRODUCT="DRUM"
  PICO_PROGRAM_DESCRIPTION="SB25 DRUM proto"
  PICO_PROGRAM_VERSION_STRING="${GIT_HASH}"
)

add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD
    COMMAND ${CMAKE_SIZE_UTIL} -A $<TARGET_FILE:${EXECUTABLE_NAME}>
    COMMENT "Detailed size report:"
)

add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD
    COMMAND ${CMAKE_SIZE_UTIL} $<TARGET_FILE:${EXECUTABLE_NAME}>
    COMMENT "Concise size report:"
)

musin_init(${EXECUTABLE_NAME})
musin_init_drivers(${EXECUTABLE_NAME})
musin_init_ui(${EXECUTABLE_NAME})
musin_init_hal(${EXECUTABLE_NAME})
musin_init_usb_midi(${EXECUTABLE_NAME})

pico_enable_stdio_usb(${EXECUTABLE_NAME} 1)
pico_add_extra_outputs(${EXECUTABLE_NAME})

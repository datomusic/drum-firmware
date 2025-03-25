cmake_minimum_required(VERSION 3.13...3.27)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(EXECUTABLE_NAME midi_example)

set(SDK_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../pico-sdk/)

# initialize pico-sdk from submodule
# note: this must happen before project()
include(${SDK_PATH}/pico_sdk_init.cmake)

project(midi_example)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(LIB_DIR ${CMAKE_CURRENT_LIST_DIR}/../../libraries)
set(U SB_DIR ${CMAKE_CURRENT_LIST_DIR}/../../core/usb)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

add_executable(midi_example
    main.cpp
)

target_sources(${EXECUTABLE_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../../core/midi/midi_wrapper.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../core/pico_uart.cpp
  ${USB_DIR}/usb.cpp
  ${USB_DIR}/usb_descriptors.c
  ${USB_DIR}/midi_usb_bridge/MIDIUSB.cpp
)

target_include_directories(${EXECUTABLE_NAME} PRIVATE
  ${USB_DIR}
  ${USB_DIR}/midi_usb_bridge
  ${LIB_DIR}/arduino_midi_library/src
  ${LIB_DIR}/Arduino-USBMIDI/src
)

include(${LIB_DIR}/arduino_midi_library/src/component_arduino_midi_library.cmake)


target_link_libraries(midi_example
  pico_stdlib
  tinyusb_device
  tinyusb_board
)

target_include_directories(midi_example PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/../../
)

pico_enable_stdio_usb(midi_example 1)
pico_enable_stdio_uart(midi_example 1)
pico_add_extra_outputs(midi_example)

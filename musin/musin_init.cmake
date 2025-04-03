set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(MUSIN_ROOT ${CMAKE_CURRENT_LIST_DIR})
set(MUSIN_LIBRARIES ${MUSIN_ROOT}/ports/pico/libraries)
set(MUSIN_USB ${MUSIN_ROOT}/usb)

set(SDK_PATH ${MUSIN_ROOT}/ports/pico/pico-sdk/)
set(SDK_EXTRAS_PATH ${MUSIN_ROOT}/ports/pico/pico-extras/)

# Add custom board directory before SDK init
list(APPEND PICO_BOARD_HEADER_DIRS ${MUSIN_ROOT}/boards)

# initialize pico-sdk from submodule
# note: this must happen before project()
include(${SDK_PATH}/pico_sdk_init.cmake)
include(${SDK_EXTRAS_PATH}/external/pico_extras_import.cmake)

macro(musin_init TARGET)
  target_include_directories(${TARGET} PRIVATE
    ${MUSIN_ROOT}/..
  )

  target_sources(${TARGET} PRIVATE
    ${MUSIN_ROOT}/pico_uart.cpp
  )

  target_link_libraries(${TARGET} PRIVATE
    pico_stdlib
  )

  pico_sdk_init()
  pico_enable_stdio_uart(${TARGET} 1)
  pico_enable_stdio_usb(${TARGET} 1)
  pico_add_extra_outputs(${TARGET})

endmacro()

macro(musin_init_usb_midi TARGET)

  target_include_directories(${TARGET} PRIVATE
    ${MUSIN_USB}
    ${MUSIN_USB}/midi_usb_bridge
    ${MUSIN_LIBRARIES}/arduino_midi_library/src
    ${MUSIN_LIBRARIES}/Arduino-USBMIDI/src
  )

  target_sources(${TARGET} PRIVATE
    ${MUSIN_USB}/usb.cpp
    ${MUSIN_USB}/usb_descriptors.c
    ${MUSIN_USB}/midi_usb_bridge/MIDIUSB.cpp
    ${MUSIN_ROOT}/midi/midi_wrapper.cpp
  )

  target_compile_definitions(${EXECUTABLE_NAME} PRIVATE
    PICO_STDIO_USB_ENABLE_RESET_VIA_VENDOR_INTERFACE=1
  )

  target_link_libraries(${TARGET} PRIVATE
    tinyusb_device
    tinyusb_board
  )

endmacro()

macro(musin_init_audio TARGET)
  set(MUSIN_AUDIO ${MUSIN_ROOT}/audio)

  target_sources(${TARGET} PRIVATE
    ${MUSIN_AUDIO}/audio_output.cpp
    ${MUSIN_AUDIO}/pitch_shifter.cpp
    ${MUSIN_AUDIO}/audio_memory_reader.cpp
    ${MUSIN_AUDIO}/data_ulaw.c
    ${MUSIN_AUDIO}/mixer.cpp
    ${MUSIN_AUDIO}/aic3204.c
  )

  target_compile_definitions(${TARGET} PRIVATE
    PICO_AUDIO_I2S_MONO_INPUT=1
    USE_AUDIO_I2S=1
  )

  target_link_libraries(${TARGET} PRIVATE
    hardware_dma
    hardware_pio
    hardware_i2c
    hardware_irq
    pico_audio_i2s
  )
endmacro()

macro(musin_init_filesystem TARGET)

  add_subdirectory(${MUSIN_ROOT}/ports/pico/libraries/pico-vfs vfs_build)

  target_sources(${TARGET} PRIVATE
    ${MUSIN_ROOT}/filesystem/filesystem.cpp
  )

  target_link_libraries(${TARGET} PRIVATE
    filesystem_vfs
  )

  pico_enable_filesystem(${TARGET})

  # Disable warnings since pico_extras audio causes warnings, which error because of -Werror in pico-vfs.
  target_compile_options(${TARGET} PRIVATE -w)
endmacro()

macro(musin_init_ui TARGET)
  set(MUSIN_UI ${MUSIN_ROOT}/ui)

  target_sources(${TARGET} PRIVATE
    ${MUSIN_UI}/keypad_hc138.cpp
  )

  target_link_libraries(${TARGET} PRIVATE
    hardware_gpio
  )
endmacro()

macro(musin_init_hal TARGET)
  set(MUSIN_HAL ${MUSIN_ROOT}/hal)

  target_sources(${TARGET} PRIVATE
    ${MUSIN_HAL}/ws2812.cpp
    ${MUSIN_HAL}/analog_in.cpp
  )

  pico_generate_pio_header(${TARGET} ${MUSIN_HAL}/ws2812.pio)

  target_link_libraries(${TARGET} PRIVATE
    hardware_pio
    hardware_dma
    hardware_adc
  )
endmacro()

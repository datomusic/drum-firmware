set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_CURRENT_LIST_DIR}/generic_sources.cmake)


set(SDK_PATH ${MUSIN_ROOT}/ports/pico/pico-sdk/)
set(SDK_EXTRAS_PATH ${MUSIN_ROOT}/ports/pico/pico-extras/)

# Add custom board directory before SDK init
list(APPEND PICO_BOARD_HEADER_DIRS ${MUSIN_ROOT}/boards)

# initialize pico-sdk from submodule
# note: this must happen before project()
include(${SDK_PATH}/pico_sdk_init.cmake)
include(${SDK_EXTRAS_PATH}/external/pico_extras_import.cmake)

if(NOT TARGET etl::etl)
  add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../lib/etl etl_build)
endif()

# this must be called after pico_sdk_init.cmake is included
pico_sdk_init()

macro(musin_init TARGET)
  target_include_directories(${TARGET} PRIVATE
    ${MUSIN_ROOT}/..
    ${MUSIN_ROOT}/ports/pico
  )

  target_sources(${TARGET} PRIVATE
    ${MUSIN_ROOT}/pico_uart.cpp
    ${MUSIN_ROOT}/timing/internal_clock.cpp
    ${MUSIN_ROOT}/timing/sync_out.cpp
  )

  target_link_libraries(${TARGET} PRIVATE
    pico_stdlib
    etl::etl
  )
  # pico_enable_stdio_uart(${TARGET} 1)
  pico_enable_stdio_usb(${TARGET} 1)
  pico_add_extra_outputs(${TARGET})

  target_compile_options(${TARGET} PRIVATE -Wall -Wextra)

  set_source_files_properties(${SDK_EXTRAS_PATH}/src/rp2_common/pico_audio_i2s/audio_i2s.c PROPERTIES COMPILE_FLAGS -Wno-unused-parameter)
  set_source_files_properties(${SDK_EXTRAS_PATH}/src/common/pico_audio/audio.cpp PROPERTIES COMPILE_FLAGS -Wno-missing-field-initializers)

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
    ${MUSIN_ROOT}/ports/pico/port/midi_wrapper.cpp
    ${MUSIN_ROOT}/midi/midi_message_queue.cpp
    ${MUSIN_ROOT}/timing/midi_clock_processor.cpp
  )

  target_compile_definitions(${TARGET} PRIVATE
    PICO_STDIO_USB_ENABLE_RESET_VIA_VENDOR_INTERFACE=1
  )

  target_link_libraries(${TARGET} PRIVATE
    tinyusb_device
    tinyusb_board
  )

endmacro()

macro(musin_init_audio TARGET)
  target_sources(${TARGET} PRIVATE
    ${musin_audio_generic_sources}
    ${MUSIN_AUDIO}/audio_output.cpp
    ${MUSIN_AUDIO}/unbuffered_file_sample_reader.cpp
    ${MUSIN_DRIVERS}/aic3204.cpp
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
    hardware_interp
  )
endmacro()

# --- Filesystem ---
macro(musin_setup_filesystem_target)
    if(NOT TARGET filesystem_vfs)
        add_subdirectory(${MUSIN_ROOT}/ports/pico/libraries/pico-vfs vfs_build)
    endif()

    # Private implementation library for musin filesystem
    add_library(musin_filesystem_impl STATIC
        ${MUSIN_ROOT}/filesystem/filesystem.cpp
    )

    # Implementation needs include paths to find musin headers
    target_include_directories(musin_filesystem_impl PRIVATE
        ${MUSIN_ROOT}/..
        ${MUSIN_ROOT}/ports/pico/libraries/pico-vfs/littlefs
    )

    # Implementation needs pico stdlib and the vfs library to compile
    target_link_libraries(musin_filesystem_impl PRIVATE
        pico_stdlib
        filesystem_vfs
    )

    # Public interface library for filesystem
    add_library(musin_filesystem INTERFACE)
    target_link_libraries(musin_filesystem INTERFACE
        musin_filesystem_impl
        filesystem_vfs
    )
    add_library(musin::filesystem ALIAS musin_filesystem)
endmacro()

macro(musin_init_ui TARGET)
  target_sources(${TARGET} PRIVATE
    ${MUSIN_UI}/analog_control.cpp
    ${MUSIN_UI}/button.cpp
  )

  target_link_libraries(${TARGET} PRIVATE
    hardware_gpio
  )
endmacro()

macro(musin_init_hal TARGET)
  set(MUSIN_HAL ${MUSIN_ROOT}/hal)

  target_sources(${TARGET} PRIVATE
    ${MUSIN_HAL}/analog_in.cpp
    ${MUSIN_HAL}/gpio.cpp
  )

  target_link_libraries(${TARGET} PRIVATE
    hardware_pio
    hardware_dma
    hardware_adc
  )
endmacro()

macro(musin_init_drivers TARGET)
    pico_generate_pio_header(${TARGET} ${MUSIN_DRIVERS}/ws2812.pio)

    target_link_libraries(${TARGET} PRIVATE
      hardware_pio
      hardware_dma
    )
endmacro()

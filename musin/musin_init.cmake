set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(MUSIN_ROOT ${CMAKE_CURRENT_LIST_DIR})

set(SDK_PATH ${MUSIN_ROOT}/ports/pico/pico-sdk/)
set(SDK_EXTRAS_PATH ${MUSIN_ROOT}/ports/pico/pico-extras/)

# initialize pico-sdk from submodule
# note: this must happen before project()
include(${SDK_PATH}/pico_sdk_init.cmake)
include(${SDK_EXTRAS_PATH}/external/pico_extras_import.cmake)

macro(musin_init TARGET)

  set(MUSIN_SHARED ${MUSIN_ROOT}/shared)
  set(LIB_DIR ${MUSIN_ROOT}/ports/pico/libraries)
  set(MUSIN_AUDIO ${MUSIN_SHARED}/audio)
  set(MUSIN_USB ${MUSIN_SHARED}/usb)

  add_subdirectory(${LIB_DIR}/pico-vfs build)

  target_include_directories(${TARGET} PRIVATE
    ${MUSIN_SHARED}
    ${MUSIN_USB}
    ${LIB_DIR}/arduino_midi_library/src
    ${LIB_DIR}/Arduino-USBMIDI/src
  )

  target_sources(${TARGET} PRIVATE
    ${MUSIN_AUDIO}/audio_output.cpp
    ${MUSIN_AUDIO}/pitch_shifter.cpp
    ${MUSIN_AUDIO}/audio_memory_reader.cpp
    ${MUSIN_AUDIO}/data_ulaw.c
    ${MUSIN_AUDIO}/mixer.cpp
    # ${MUSIN_USB}/usb.cpp
    # ${MUSIN_USB}/usb_descriptors.c
    # ${MUSIN_USB}/midi_usb_bridge/MIDIUSB.cpp
    # ${MUSIN_SHARED}/filesystem/filesystem.c
  )

  target_compile_definitions(${TARGET} PRIVATE
    PICO_AUDIO_I2S_MONO_INPUT=1
    USE_AUDIO_I2S=1
  )

  target_link_libraries(${TARGET} PRIVATE
    pico_stdlib
    hardware_dma
    hardware_pio
    hardware_irq
    pico_audio_i2s
    tinyusb_device
    tinyusb_board
  )

  pico_sdk_init()
  pico_add_extra_outputs(${TARGET})

  # pico_enable_filesystem(${EXECUTABLE_NAME})

endmacro()

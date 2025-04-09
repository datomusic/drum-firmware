# This file previously contained initialization macros.
# Its functionality is being migrated to musin/CMakeLists.txt
# which defines library targets (e.g., musin::core, musin::hal).
#
# Applications should now use add_subdirectory(path/to/musin)
# and link against the specific musin::* libraries they need.

# Keep remaining macros for now, they will be refactored into library definitions later.

# Define path variables needed by remaining macros
set(MUSIN_ROOT ${CMAKE_CURRENT_LIST_DIR})
set(MUSIN_LIBRARIES ${MUSIN_ROOT}/ports/pico/libraries)
set(MUSIN_USB ${MUSIN_ROOT}/usb)
set(MUSIN_DRIVERS ${MUSIN_ROOT}/drivers)
set(MUSIN_AUDIO ${MUSIN_ROOT}/audio)
set(MUSIN_UI ${MUSIN_ROOT}/ui)
set(MUSIN_HAL ${MUSIN_ROOT}/hal)
set(SDK_EXTRAS_PATH ${MUSIN_ROOT}/ports/pico/pico-extras/) # Keep for warning suppression


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
    ${MUSIN_AUDIO}/crusher.cpp
    ${MUSIN_AUDIO}/waveshaper.cpp
    ${MUSIN_AUDIO}/filter.cpp
    ${MUSIN_DRIVERS}/aic3204.c # Codec-specific driver, but the only audio codec we are using currently.
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
    ${MUSIN_HAL}/analog_in.cpp
  )

  target_link_libraries(${TARGET} PRIVATE
    hardware_pio
    hardware_dma
    hardware_adc
  )
endmacro()                                                                                                                                                                      
                                                                                                                                                                                
                                                                                                                                                                                
macro(musin_init_drivers TARGET)                                                                                                                                                
                                        
    target_sources(${TARGET} PRIVATE                                                                                                                                              
      ${MUSIN_DRIVERS}/ws2812.cpp                                                                                                                                                 
    )                                                                                                                                                                             
                                                                                                                                                                                  
    # PIO generation should be handled per-target in the target's CMakeLists.txt
    # pico_generate_pio_header(${TARGET} ${MUSIN_DRIVERS}/ws2812.pio)                                                                                                               
                                                                                                                                                                                  
    target_link_libraries(${TARGET} PRIVATE                                                                                                                                       
      hardware_pio # For ws2812                                                                                                                                                   
      hardware_dma # For ws2812 (potentially, or for PIO interaction)                                                                                                             
    )                                                                                                                                                                             
  endmacro()             

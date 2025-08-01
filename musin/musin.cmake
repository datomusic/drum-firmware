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

# These properties seem to apply globally when using pico audio extras.
set_source_files_properties(${SDK_EXTRAS_PATH}/src/rp2_common/pico_audio_i2s/audio_i2s.c PROPERTIES COMPILE_FLAGS -Wno-unused-parameter)
set_source_files_properties(${SDK_EXTRAS_PATH}/src/common/pico_audio/audio.cpp PROPERTIES COMPILE_FLAGS -Wno-missing-field-initializers)

macro(musin_setup_core_target)
    # Private implementation library for musin core
    add_library(musin_core_impl STATIC
        ${MUSIN_ROOT}/timing/internal_clock.cpp
        ${MUSIN_ROOT}/timing/sync_out.cpp
        ${MUSIN_ROOT}/timing/sync_in.cpp
        ${MUSIN_ROOT}/timing/tempo_handler.cpp
    )

    # Implementation needs access to its own headers
    target_include_directories(musin_core_impl PRIVATE
        ${MUSIN_ROOT}/..
        ${MUSIN_ROOT}/ports/pico
    )

    # Implementation needs to link against its dependencies to compile
    target_link_libraries(musin_core_impl PRIVATE
        pico_stdlib
        etl::etl
        musin::hal # For sync_out.cpp
        musin::usb_midi
    )

    # Public interface library for core
    add_library(musin_core INTERFACE)
    target_include_directories(musin_core INTERFACE
        ${MUSIN_ROOT}/..
        ${MUSIN_ROOT}/ports/pico
    )
    target_link_libraries(musin_core INTERFACE
        musin_core_impl
        pico_stdlib
        etl::etl
        musin::hal
        musin::usb_midi
    )
    target_compile_options(musin_core INTERFACE -Wall -Wextra)
    add_library(musin::core ALIAS musin_core)
endmacro()

macro(musin_setup_usb_midi_target)
    # Private implementation library for musin usb_midi
    add_library(musin_usb_midi_impl STATIC
        ${MUSIN_USB}/usb.cpp
        ${MUSIN_USB}/midi_usb_bridge/MIDIUSB.cpp
        ${MUSIN_ROOT}/ports/pico/port/midi_wrapper.cpp
        ${MUSIN_ROOT}/midi/midi_output_queue.cpp
        ${MUSIN_ROOT}/midi/midi_input_queue.cpp
        ${MUSIN_ROOT}/timing/midi_clock_processor.cpp
    )

    # Implementation needs include paths to find its headers and dependencies
    target_include_directories(musin_usb_midi_impl PRIVATE
        ${MUSIN_ROOT}/..
        ${MUSIN_USB}
        ${MUSIN_USB}/midi_usb_bridge
        ${MUSIN_LIBRARIES}/arduino_midi_library/src
        ${MUSIN_LIBRARIES}/Arduino-USBMIDI/src
    )

    # Implementation needs pico stdlib and etl
    target_link_libraries(musin_usb_midi_impl PRIVATE
        pico_stdlib
        pico_stdio_usb
        tinyusb_device
        tinyusb_board
        etl::etl
    )

    # Public interface library for usb_midi
    add_library(musin_usb_midi INTERFACE)
    target_include_directories(musin_usb_midi INTERFACE
        ${MUSIN_USB}
        ${MUSIN_USB}/midi_usb_bridge
        ${MUSIN_LIBRARIES}/arduino_midi_library/src
        ${MUSIN_LIBRARIES}/Arduino-USBMIDI/src
    )
    target_link_libraries(musin_usb_midi INTERFACE
        musin_usb_midi_impl
        tinyusb_device
        tinyusb_board
        pico_stdio_usb
        etl::etl # midi_clock_processor.h and others use etl
    )

    target_compile_definitions(musin_usb_midi INTERFACE
        PICO_STDIO_USB_ENABLE_RESET_VIA_VENDOR_INTERFACE=1
    )

    add_library(musin::usb_midi ALIAS musin_usb_midi)
endmacro()

macro(musin_setup_audio_target)
    # Private implementation library for musin audio
    add_library(musin_audio_impl STATIC
        ${musin_audio_generic_sources}
        ${MUSIN_AUDIO}/audio_output.cpp
        ${MUSIN_AUDIO}/unbuffered_file_sample_reader.cpp
        ${MUSIN_DRIVERS}/aic3204.cpp
    )

    # Implementation needs include paths to find its own headers and dependencies
    target_include_directories(musin_audio_impl PRIVATE
        ${MUSIN_ROOT}/..
        ${MUSIN_ROOT}/ports/pico
    )

    target_compile_definitions(musin_audio_impl PRIVATE
        AUDIO_BLOCK_SAMPLES=128
    )

    # Implementation needs to link against its dependencies to compile
    target_link_libraries(musin_audio_impl PRIVATE
        pico_stdlib
        etl::etl
        hardware_dma
        hardware_pio
        hardware_i2c
        hardware_irq
        pico_audio_i2s
        hardware_interp
    )

    # Public interface library for audio
    add_library(musin_audio INTERFACE)
    target_link_libraries(musin_audio INTERFACE
        musin_audio_impl
        etl::etl # for AudioBlock and other headers using etl
        hardware_dma
        hardware_pio
        hardware_i2c
        hardware_irq
        pico_audio_i2s
        hardware_interp
    )

    target_compile_definitions(musin_audio INTERFACE
        PICO_AUDIO_I2S_MONO_INPUT=1
        USE_AUDIO_I2S=1
        AUDIO_BLOCK_SAMPLES=128
        PICO_AUDIO_I2S_CLOCK_PINS_SWAPPED=1
    )

    add_library(musin::audio ALIAS musin_audio)
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
        ${MUSIN_ROOT}/ports/pico/libraries/pico-vfs/vendor/littlefs
        ${MUSIN_ROOT}/ports/pico/pico-sdk/src/rp2_common/hardware_flash/include
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

macro(musin_setup_ui_target)
    # Private implementation library for musin ui
    add_library(musin_ui_impl STATIC
        ${MUSIN_UI}/adaptive_filter.cpp
        ${MUSIN_UI}/analog_control.cpp
        ${MUSIN_UI}/button.cpp
        ${MUSIN_UI}/drumpad.cpp
    )

    # Implementation needs include paths to find musin headers
    target_include_directories(musin_ui_impl PRIVATE
        ${MUSIN_ROOT}/..
    )

    # Implementation needs pico stdlib and etl
    target_link_libraries(musin_ui_impl PRIVATE
        pico_stdlib
        etl::etl
    )

    # Public interface library for ui
    add_library(musin_ui INTERFACE)
    target_link_libraries(musin_ui INTERFACE
        musin_ui_impl
        hardware_gpio
        etl::etl
    )
    add_library(musin::ui ALIAS musin_ui)
endmacro()

macro(musin_setup_hal_target)
    # Private implementation library for musin hal
    add_library(musin_hal_impl STATIC
        ${MUSIN_ROOT}/hal/gpio.cpp
        ${MUSIN_ROOT}/hal/null_logger.cpp
        ${MUSIN_ROOT}/hal/pico_logger.cpp
        ${MUSIN_ROOT}/hal/analog_mux_scanner.cpp
        ${MUSIN_ROOT}/hal/adc_defs.cpp
    )

    # Implementation needs include paths to find musin headers
    target_include_directories(musin_hal_impl PRIVATE
        ${MUSIN_ROOT}/..
    )

    # Implementation needs pico stdlib for gpio/adc functionality
    target_link_libraries(musin_hal_impl PRIVATE
        pico_stdlib
        hardware_adc
        hardware_gpio
        etl::etl
    )

    # Public interface library for hal
    add_library(musin_hal INTERFACE)
    target_link_libraries(musin_hal INTERFACE
        musin_hal_impl
        hardware_pio
        hardware_dma
        hardware_adc
        etl::etl
    )
    add_library(musin::hal ALIAS musin_hal)
endmacro()

macro(musin_setup_drivers_target)
    # Private implementation library for musin drivers, primarily for PIO header generation
    file(WRITE ${CMAKE_BINARY_DIR}/musin_drivers_dummy.cpp "")
    add_library(musin_drivers_impl STATIC ${CMAKE_BINARY_DIR}/musin_drivers_dummy.cpp)

    pico_generate_pio_header(musin_drivers_impl ${MUSIN_DRIVERS}/ws2812.pio)

    # Public interface library for drivers
    add_library(musin_drivers INTERFACE)
    target_link_libraries(musin_drivers INTERFACE
        musin_drivers_impl
        hardware_pio
        hardware_dma
    )
    add_library(musin::drivers ALIAS musin_drivers)
endmacro()

cmake_minimum_required(VERSION 3.13...3.27)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SDK_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../pico-sdk/)
set(SDK_EXTRAS_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../pico-extras/)

# initialize pico-sdk from submodule
# note: this must happen before project()
include(${SDK_PATH}/pico_sdk_init.cmake)
include(${SDK_EXTRAS_PATH}/external/pico_extras_import.cmake)


project(sine_test)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

add_executable(sine_test
    main.cpp
)

target_link_libraries(sine_test
  pico_stdlib
  hardware_dma
  hardware_pio
  hardware_irq
  pico_audio_i2s
)
target_compile_definitions(sine_test PRIVATE
  # compile time configuration of I2S
  PICO_AUDIO_I2S_MONO_INPUT=1
  USE_AUDIO_I2S=1
  PICO_AUDIO_I2S_DATA_PIN=18
  PICO_AUDIO_I2S_CLOCK_PIN_BASE=16
# PICO_DEFAULT_UART=0
# PICO_DEFAULT_UART_TX_PIN=28
# PICO_DEFAULT_UART_RX_PIN=29
  )

pico_enable_stdio_usb(sine_test 1)
pico_enable_stdio_uart(sine_test 1)
pico_add_extra_outputs(sine_test)

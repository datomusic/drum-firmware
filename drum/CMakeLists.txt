cmake_minimum_required(VERSION 3.13...3.27)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(EXECUTABLE_NAME drum-brains)

include(${CMAKE_CURRENT_SOURCE_DIR}/../musin/musin_init.cmake)

project(${EXECUTABLE_NAME})

add_executable(${EXECUTABLE_NAME}
  main.cpp
)

# target_include_directories(${EXECUTABLE_NAME} PRIVATE
# )

target_compile_definitions(${EXECUTABLE_NAME} PRIVATE
  AUDIO_BLOCK_SAMPLES=32
  PICO_AUDIO_I2S_DATA_PIN=18
  PICO_AUDIO_I2S_CLOCK_PIN_BASE=16
  # PICO_STDIO_USB_ENABLE_RESET_VIA_VENDOR_INTERFACE=1
)

musin_init(${EXECUTABLE_NAME})
musin_init_audio(${EXECUTABLE_NAME})
musin_init_usb_midi(${EXECUTABLE_NAME})
musin_init_filesystem(${EXECUTABLE_NAME})

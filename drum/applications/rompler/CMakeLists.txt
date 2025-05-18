cmake_minimum_required(VERSION 3.13...3.27)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(${CMAKE_CURRENT_LIST_DIR}/../../../musin/musin_init.cmake)

project(rompler)

add_executable(${PROJECT_NAME}
  main.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../..
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
  PICO_STDIO_USB_ENABLE_RESET_VIA_VENDOR_INTERFACE=1
  AUDIO_BLOCK_SAMPLES=128
)

if(SWAP_AUDIO_CLOCK)
  target_compile_definitions(${PROJECT_NAME} PRIVATE
    PICO_AUDIO_I2S_CLOCK_PINS_SWAPPED=1
  )
endif(SWAP_AUDIO_CLOCK)

musin_init(${PROJECT_NAME})
musin_init_usb_midi(${PROJECT_NAME})
musin_init_audio(${PROJECT_NAME})
musin_init_filesystem(${PROJECT_NAME})

pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_add_extra_outputs(${PROJECT_NAME})

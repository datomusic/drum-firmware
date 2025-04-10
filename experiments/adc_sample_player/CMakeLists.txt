cmake_minimum_required(VERSION 3.13...3.27)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PICO_BOARD dato_submarine)

set(EXECUTABLE_NAME adc_sample_player)

set(EXAMPLES_SUPPORT_PATH ${CMAKE_CURRENT_LIST_DIR}/../support/)

include(${CMAKE_CURRENT_LIST_DIR}/../../musin/musin_init.cmake)

project(${EXECUTABLE_NAME})

add_executable(${EXECUTABLE_NAME}
  main.cpp
  ${EXAMPLES_SUPPORT_PATH}/samples/AudioSampleKickc78_16bit_44kw.cpp
  ${EXAMPLES_SUPPORT_PATH}/samples/AudioSampleHatdr55_16bit_44kw.cpp
  ${EXAMPLES_SUPPORT_PATH}/samples/AudioSampleSnare100_16bit_44kw.cpp
  ${EXAMPLES_SUPPORT_PATH}/samples/AudioSampleClapdr110_16bit_44kw.cpp
)

target_include_directories(${EXECUTABLE_NAME} PRIVATE
  ${EXAMPLES_SUPPORT_PATH}
)

target_compile_definitions(${EXECUTABLE_NAME} PRIVATE
  AUDIO_BLOCK_SAMPLES=32
)


if(SWAP_AUDIO_CLOCK)
  target_compile_definitions(${EXECUTABLE_NAME} PRIVATE
    PICO_AUDIO_I2S_CLOCK_PINS_SWAPPED=1
  )
endif(SWAP_AUDIO_CLOCK)

musin_init(${EXECUTABLE_NAME})
musin_init_hal(${EXECUTABLE_NAME})
musin_init_drivers(${EXECUTABLE_NAME})
musin_init_audio(${EXECUTABLE_NAME})
musin_init_usb_midi(${EXECUTABLE_NAME})

cmake_minimum_required(VERSION 3.13...3.27)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(EXECUTABLE_NAME flash_audio_streaming)

set(EXAMPLES_SUPPORT_PATH ${CMAKE_CURRENT_LIST_DIR}/../support/)

include(${CMAKE_CURRENT_LIST_DIR}/../../musin/musin_init.cmake)

project(${EXECUTABLE_NAME})

add_executable(${EXECUTABLE_NAME}
  main.cpp
  ${EXAMPLES_SUPPORT_PATH}/samples/AudioSampleKick.cpp
  ${EXAMPLES_SUPPORT_PATH}/samples/AudioSampleSnare.cpp
  ${EXAMPLES_SUPPORT_PATH}/samples/AudioSampleGong.cpp
  ${EXAMPLES_SUPPORT_PATH}/samples/AudioSampleCashregister.cpp
  ${EXAMPLES_SUPPORT_PATH}/samples/AudioSampleHihat.cpp
)

target_include_directories(${EXECUTABLE_NAME} PRIVATE
  ${EXAMPLES_SUPPORT_PATH}
)

target_compile_definitions(${EXECUTABLE_NAME} PRIVATE
  AUDIO_BLOCK_SAMPLES=32
)

if(DATO_SUBMARINE)
  target_sources(${EXECUTABLE_NAME} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../../musin/audio/aic3204.c
  )
endif()

musin_init(${EXECUTABLE_NAME})
musin_init_audio(${EXECUTABLE_NAME})
musin_init_usb_midi(${EXECUTABLE_NAME})
musin_init_filesystem(${EXECUTABLE_NAME})

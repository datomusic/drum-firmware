cmake_minimum_required(VERSION 3.13...3.27)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PICO_BOARD dato_submarine)

set(EXECUTABLE_NAME midi_sample_player)

set(EXAMPLES_SUPPORT_PATH ${CMAKE_CURRENT_LIST_DIR}/../support/)

include(${CMAKE_CURRENT_LIST_DIR}/../../musin/musin_init.cmake)

project(${EXECUTABLE_NAME})

add_executable(${EXECUTABLE_NAME}
  # == Add all 32 generated .c files ==
  # == Replace SampleNameX_data.c with actual filenames ==
  ${CMAKE_CURRENT_LIST_DIR}/samples/005_data.c
  ${CMAKE_CURRENT_LIST_DIR}/samples/006_data.c
  ${CMAKE_CURRENT_LIST_DIR}/samples/015_data.c
  ${CMAKE_CURRENT_LIST_DIR}/samples/100_nt_snare_data.c
  ${CMAKE_CURRENT_LIST_DIR}/samples/26880__vexst__closed-hi-hat-2-1_data.c
  ${CMAKE_CURRENT_LIST_DIR}/samples/26887__vexst__kick-3-1_data.c
  ${CMAKE_CURRENT_LIST_DIR}/samples/26901__vexst__snare-2-1_data.c
  ${CMAKE_CURRENT_LIST_DIR}/samples/44-Analog_Cowbell_data.c
  ${CMAKE_CURRENT_LIST_DIR}/samples/cabasa_data.c
  ${CMAKE_CURRENT_LIST_DIR}/samples/chihiro_snare_data.c
  ${CMAKE_CURRENT_LIST_DIR}/samples/cowbell_hi_data.c
  ${CMAKE_CURRENT_LIST_DIR}/samples/ClosedHH_909X_2_data.c
  ${CMAKE_CURRENT_LIST_DIR}/samples/DR110_clap_data.c
  ${CMAKE_CURRENT_LIST_DIR}/samples/DR55HAT_data.c
  ${CMAKE_CURRENT_LIST_DIR}/samples/DR55RIM_data.c
  ${CMAKE_CURRENT_LIST_DIR}/samples/duo_hat_01_data.c
  ${CMAKE_CURRENT_LIST_DIR}/samples/duo_kick_01_data.c
  ${CMAKE_CURRENT_LIST_DIR}/samples/DUO_snare_01_data.c
  ${CMAKE_CURRENT_LIST_DIR}/samples/FR_BB_Sarik_HHat_010-1_data.c
  ${CMAKE_CURRENT_LIST_DIR}/samples/FR_BB_Sarik_Snare_004-1_data.c
  ${CMAKE_CURRENT_LIST_DIR}/samples/Finger_Snap_data.c
  ${CMAKE_CURRENT_LIST_DIR}/samples/JR_SDD_HAT_A1_mono_data.c
  ${CMAKE_CURRENT_LIST_DIR}/samples/JR_SDD_KICK_1-1_data.c
  ${CMAKE_CURRENT_LIST_DIR}/samples/JR_SDD_SNARE_10_data.c
  ${CMAKE_CURRENT_LIST_DIR}/samples/KEMP8_SET1_54-004_data.c
  ${CMAKE_CURRENT_LIST_DIR}/samples/Kick-C78_data.c
  ${CMAKE_CURRENT_LIST_DIR}/samples/Kick_909_23_data.c
  ${CMAKE_CURRENT_LIST_DIR}/samples/skclhat_data.c
  ${CMAKE_CURRENT_LIST_DIR}/samples/Snare_909_3_data.c
  ${CMAKE_CURRENT_LIST_DIR}/samples/Snare-C78-with-silence_data.c
  ${CMAKE_CURRENT_LIST_DIR}/samples/vocal_3_data.c
  ${CMAKE_CURRENT_LIST_DIR}/samples/Zap_2_data.c
  # Then main source
  main.cpp
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
musin_init_drivers(${EXECUTABLE_NAME})
musin_init_audio(${EXECUTABLE_NAME})
musin_init_usb_midi(${EXECUTABLE_NAME})

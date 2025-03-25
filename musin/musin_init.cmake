set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)


set(MUSIN_ROOT ${CMAKE_CURRENT_LIST_DIR})

macro(musin_init TARGET)
  target_include_directories(${TARGET} PRIVATE
    ${MUSIN_ROOT}/shared/
  )

  target_sources(${TARGET} PRIVATE
    ${MUSIN_ROOT}/shared/audio/audio_output.cpp
    ${MUSIN_ROOT}/shared/audio/pitch_shifter.cpp
    ${MUSIN_ROOT}/shared/audio/audio_memory_reader.cpp
    ${MUSIN_ROOT}/shared/audio/data_ulaw.c
    ${MUSIN_ROOT}/shared/audio/mixer.cpp
  )

  target_compile_definitions(${TARGET} PRIVATE
    PICO_AUDIO_I2S_MONO_INPUT=1
    USE_AUDIO_I2S=1
  )

  target_link_libraries(${TARGET}
    pico_stdlib
    hardware_dma
    hardware_pio
    hardware_irq
    pico_audio_i2s
  )
endmacro()



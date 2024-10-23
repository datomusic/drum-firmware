CMAKE_MINIMUM_REQUIRED (VERSION 3.13.0)
project(sample-player-tests)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_FLAGS "-fsanitize=address -fno-omit-frame-pointer -g3")
set(CMAKE_CXX_FLAGS "-fsanitize=address -fno-omit-frame-pointer -g3")

add_subdirectory(Catch2)
add_executable(tests 
    pitch_shifter_test.cpp
    ../pitch_shifter.cpp
    ../AudioSampleKick.cpp
    ../audio_memory_reader.cpp
    ./../../../libraries/Teensy_Audio/data_ulaw.c
  )

target_include_directories(tests PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/..
)

target_link_libraries(tests PRIVATE Catch2::Catch2WithMain)
include(CTest)
include(Catch)
catch_discover_tests(tests)

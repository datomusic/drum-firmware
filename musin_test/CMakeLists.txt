CMAKE_MINIMUM_REQUIRED (VERSION 3.13.0)
project(musin-test)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_FLAGS "-fsanitize=address -fno-omit-frame-pointer -g3")
set(CMAKE_CXX_FLAGS "-fsanitize=address -fno-omit-frame-pointer -g3")

add_subdirectory(Catch2)
# find_package(Catch2 3 REQUIRED)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../lib/etl etl_build)

add_executable(tests 
    pitch_shifter_test.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../musin/audio/pitch_shifter.cpp
    # ../AudioSampleKick.cpp
    # ../audio_memory_reader.cpp
    # ./../../../libraries/Teensy_Audio/data_ulaw.c
  )

target_include_directories(tests PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/..
)

add_definitions(
    -DAUDIO_BLOCK_SAMPLES=20
)

target_link_libraries(tests PRIVATE 
  Catch2::Catch2WithMain
  etl::etl
)

include(CTest)
include(Catch)
catch_discover_tests(tests)

CMAKE_MINIMUM_REQUIRED (VERSION 3.13.0)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_FLAGS "-fsanitize=address -fno-omit-frame-pointer -g3")
set(CMAKE_CXX_FLAGS "-fsanitize=address -fno-omit-frame-pointer -g3")

set(CMAKE_CXX_STANDARD 20)

project(musin-test)

include(${CMAKE_CURRENT_LIST_DIR}/../../musin/generic_sources.cmake)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../lib/etl etl_build)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../lib/test/Catch2 ${CMAKE_CURRENT_LIST_DIR}/Catch2_build)

add_executable(${PROJECT_NAME}
  audio/pitch_shifter_test.cpp
  audio/memory_reader_test.cpp
  hal/mock_hardware.cpp
  hal/analog_in_test.cpp
  ui/keypad_hc138_test.cpp
)

if(${STATIC_TESTS})
  message("CMake: Building static tests")
  target_compile_definitions(${PROJECT_NAME} PRIVATE
    STATIC_TESTS=1
  )
endif(${STATIC_TESTS})

target_sources(${PROJECT_NAME} PRIVATE
  ${musin_audio_generic_sources}
  ${CMAKE_CURRENT_LIST_DIR}/../../musin/hal/analog_in.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/..
  ${CMAKE_CURRENT_LIST_DIR}/../..
)

add_definitions(
  -DAUDIO_BLOCK_SAMPLES=20
)

target_link_libraries(${PROJECT_NAME} PRIVATE
  Catch2::Catch2WithMain
  etl::etl
)

include(CTest)
include(Catch)
catch_discover_tests(${PROJECT_NAME})

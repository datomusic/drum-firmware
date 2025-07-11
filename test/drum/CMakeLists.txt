CMAKE_MINIMUM_REQUIRED(VERSION 3.13.0)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_FLAGS "-fsanitize=address -fno-omit-frame-pointer -g3")
set(CMAKE_CXX_FLAGS "-fsanitize=address -fno-omit-frame-pointer -g3")

set(CMAKE_CXX_STANDARD 20)

project(drum-test)

include(${CMAKE_CURRENT_LIST_DIR}/../../musin/generic_sources.cmake)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../lib/etl etl_build)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../lib/test/Catch2 ${CMAKE_CURRENT_LIST_DIR}/Catch2_build)

add_executable(${PROJECT_NAME}
    message_router_test.cpp
)

target_sources(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../../drum/message_router.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../drum/audio_engine.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../drum/sequencer_controller.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../musin/midi/midi_output_queue.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../midi_test_support.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../musin/include_overrides
    ${CMAKE_CURRENT_LIST_DIR}/../../musin/ports/pico/libraries/arduino_midi_library/src
    ${CMAKE_CURRENT_LIST_DIR}/../../
    ${CMAKE_CURRENT_LIST_DIR}/../
    ${CMAKE_CURRENT_LIST_DIR}/../musin/include_overrides
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    AUDIO_BLOCK_SAMPLES=20
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Catch2::Catch2WithMain
    etl::etl
)

include(CTest)
include(Catch)
catch_discover_tests(${PROJECT_NAME})
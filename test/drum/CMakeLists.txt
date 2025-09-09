CMAKE_MINIMUM_REQUIRED(VERSION 3.13.0)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_FLAGS "-fsanitize=address -fno-omit-frame-pointer -g3")
set(CMAKE_CXX_FLAGS "-fsanitize=address -fno-omit-frame-pointer -g3")

set(CMAKE_CXX_STANDARD 20)

project(drum-tests)

include(${CMAKE_CURRENT_LIST_DIR}/../../musin/generic_sources.cmake)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../lib/etl etl_build)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../lib/test/Catch2 ${CMAKE_CURRENT_LIST_DIR}/Catch2_build)

# Test target: Sequencer Storage (uses inline mocks in the test file)
add_executable(drum-test-storage
    sequencer_storage_test.cpp
)

target_sources(drum-test-storage PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../../drum/sequencer_storage.cpp
)

target_include_directories(drum-test-storage PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../musin/include_overrides
    ${CMAKE_CURRENT_LIST_DIR}/../../musin/ports/pico/libraries/arduino_midi_library/src
    ${CMAKE_CURRENT_LIST_DIR}/../../
    ${CMAKE_CURRENT_LIST_DIR}/../
    ${CMAKE_CURRENT_LIST_DIR}/../musin/include_overrides
)

target_compile_definitions(drum-test-storage PRIVATE
    AUDIO_BLOCK_SAMPLES=20
)

target_link_libraries(drum-test-storage PRIVATE
    Catch2::Catch2WithMain
    etl::etl
)

# Test target: Sequencer Persister (links production persister implementation)
add_executable(drum-test-persister
    sequencer_persister_test.cpp
)

target_sources(drum-test-persister PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../../drum/sequencer_persister.cpp
)

target_include_directories(drum-test-persister PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../musin/include_overrides
    ${CMAKE_CURRENT_LIST_DIR}/../../musin/ports/pico/libraries/arduino_midi_library/src
    ${CMAKE_CURRENT_LIST_DIR}/../../
    ${CMAKE_CURRENT_LIST_DIR}/../
    ${CMAKE_CURRENT_LIST_DIR}/../musin/include_overrides
)

target_compile_definitions(drum-test-persister PRIVATE
    AUDIO_BLOCK_SAMPLES=20
)

target_link_libraries(drum-test-persister PRIVATE
    Catch2::Catch2WithMain
    etl::etl
)

include(CTest)
include(Catch)
catch_discover_tests(drum-test-storage)
catch_discover_tests(drum-test-persister)

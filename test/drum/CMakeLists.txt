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

# Test target: Sequencer Effect Swing (unit tests for swing timing)
add_executable(drum-test-swing
    sequencer_effect_swing_test.cpp
)

target_sources(drum-test-swing PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../../drum/sequencer_effect_swing.cpp
)

target_include_directories(drum-test-swing PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../musin/include_overrides
    ${CMAKE_CURRENT_LIST_DIR}/../../musin/ports/pico/libraries/arduino_midi_library/src
    ${CMAKE_CURRENT_LIST_DIR}/../../
    ${CMAKE_CURRENT_LIST_DIR}/../
    ${CMAKE_CURRENT_LIST_DIR}/../musin/include_overrides
)

target_compile_definitions(drum-test-swing PRIVATE
    AUDIO_BLOCK_SAMPLES=20
)

target_link_libraries(drum-test-swing PRIVATE
    Catch2::Catch2WithMain
    etl::etl
)

# Test target: Firmware Update SysEx protocol (uses inline FakeWriter)
add_executable(drum-test-firmware-update
    firmware_update_test.cpp
)

target_include_directories(drum-test-firmware-update PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../musin/include_overrides
    ${CMAKE_CURRENT_LIST_DIR}/../../
    ${CMAKE_CURRENT_LIST_DIR}/../
)

target_link_libraries(drum-test-firmware-update PRIVATE
    Catch2::Catch2WithMain
    etl::etl
)

# Test target: Sample Slot Manager (RAM-resident sample loading)
add_executable(drum-test-sample-slots
    sample_slot_manager_test.cpp
)

target_sources(drum-test-sample-slots PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../../drum/sample_slot_manager.cpp
)

target_include_directories(drum-test-sample-slots PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../musin/include_overrides
    ${CMAKE_CURRENT_LIST_DIR}/../../
    ${CMAKE_CURRENT_LIST_DIR}/../
)

target_compile_definitions(drum-test-sample-slots PRIVATE
    AUDIO_BLOCK_SAMPLES=20
)

target_link_libraries(drum-test-sample-slots PRIVATE
    Catch2::Catch2WithMain
    etl::etl
)

# Test target: SDS protocol (sample dump standard, header-only)
add_executable(drum-test-sds
    sds_protocol_test.cpp
    sds_dump_sender_test.cpp
)

target_include_directories(drum-test-sds PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../musin/include_overrides
    ${CMAKE_CURRENT_LIST_DIR}/../../
    ${CMAKE_CURRENT_LIST_DIR}/../
)

target_link_libraries(drum-test-sds PRIVATE
    Catch2::Catch2WithMain
    etl::etl
)

# Test target: SysEx Sequencer State (codec unit tests)
add_executable(drum-test-sysex-sequencer-state
    sysex_sequencer_state_test.cpp
)

target_include_directories(drum-test-sysex-sequencer-state PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../musin/include_overrides
    ${CMAKE_CURRENT_LIST_DIR}/../../musin/ports/pico/libraries/arduino_midi_library/src
    ${CMAKE_CURRENT_LIST_DIR}/../../
    ${CMAKE_CURRENT_LIST_DIR}/../
    ${CMAKE_CURRENT_LIST_DIR}/../musin/include_overrides
)

target_compile_definitions(drum-test-sysex-sequencer-state PRIVATE
    AUDIO_BLOCK_SAMPLES=20
)

target_link_libraries(drum-test-sysex-sequencer-state PRIVATE
    Catch2::Catch2WithMain
    etl::etl
)

# Test target: Save Timing Manager (debounced sequencer state saves)
add_executable(drum-test-save-timing
    save_timing_manager_test.cpp
)

target_sources(drum-test-save-timing PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../../drum/save_timing_manager.cpp
)

target_include_directories(drum-test-save-timing PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../musin/include_overrides
    ${CMAKE_CURRENT_LIST_DIR}/../../
    ${CMAKE_CURRENT_LIST_DIR}/../
)

target_link_libraries(drum-test-save-timing PRIVATE
    Catch2::Catch2WithMain
    etl::etl
)

# Test target: SysEx file-transfer protocol (drum/sysex/protocol.h)
add_executable(drum-test-sysex
    sysex_test.cpp
)

target_include_directories(drum-test-sysex PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../musin/include_overrides
    ${CMAKE_CURRENT_LIST_DIR}/../../musin/ports/pico/libraries/arduino_midi_library/src
    ${CMAKE_CURRENT_LIST_DIR}/../../
    ${CMAKE_CURRENT_LIST_DIR}/../
)

target_compile_definitions(drum-test-sysex PRIVATE
    AUDIO_BLOCK_SAMPLES=20
)

target_link_libraries(drum-test-sysex PRIVATE
    Catch2::Catch2WithMain
    etl::etl
)

# Test target: Settings registry and validation
add_executable(drum-test-settings
    settings_test.cpp
)

target_include_directories(drum-test-settings PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../../
)

target_link_libraries(drum-test-settings PRIVATE
    Catch2::Catch2WithMain
    etl::etl
)

include(CTest)
include(Catch)
catch_discover_tests(drum-test-storage)
catch_discover_tests(drum-test-swing)
catch_discover_tests(drum-test-firmware-update)
catch_discover_tests(drum-test-sample-slots)
catch_discover_tests(drum-test-sds)
catch_discover_tests(drum-test-sysex-sequencer-state)
catch_discover_tests(drum-test-settings)
catch_discover_tests(drum-test-save-timing)
catch_discover_tests(drum-test-sysex)

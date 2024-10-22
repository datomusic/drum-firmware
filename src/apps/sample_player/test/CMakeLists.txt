CMAKE_MINIMUM_REQUIRED (VERSION 3.13.0)
project(duo-tests)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(Catch2)
add_executable(tests 
    pitch_shifter_test.cpp)

target_link_libraries(tests PRIVATE Catch2::Catch2WithMain)

include(CTest)
include(Catch)
catch_discover_tests(tests)

CMAKE_MINIMUM_REQUIRED (VERSION 3.13.0)
project(musin-test)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_FLAGS "-fsanitize=address -fno-omit-frame-pointer -g3")
set(CMAKE_CXX_FLAGS "-fsanitize=address -fno-omit-frame-pointer -g3")

set(CMAKE_CXX_STANDARD 23)

add_subdirectory(Catch2 ${CMAKE_CURRENT_LIST_DIR}/Catch2_build)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../lib/etl etl_build)

add_executable(${PROJECT_NAME}
    pitch_shifter_test.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../musin/audio/pitch_shifter.cpp
  )

target_include_directories(${PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/..
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

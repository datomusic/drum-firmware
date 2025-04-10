# This file previously contained initialization macros.
# Its functionality is being migrated to musin/CMakeLists.txt
# which defines library targets (e.g., musin::core, musin::hal).
#
# Applications should now use add_subdirectory(path/to/musin)
# and link against the specific musin::* libraries they need.

# This file is now deprecated.
# Musin components are defined as CMake targets (musin::core, musin::hal, etc.)
# in musin/CMakeLists.txt.
#
# Applications should use add_subdirectory(path/to/musin musin_build)
# and link against the specific musin::* INTERFACE libraries they need in
# their own CMakeLists.txt file using target_link_libraries().
#
# Example application CMakeLists.txt:
#
# cmake_minimum_required(VERSION 3.13)
# include(pico_sdk_import.cmake)
# project(my_musin_app C CXX ASM)
# set(CMAKE_C_STANDARD 11)
# set(CMAKE_CXX_STANDARD 20)
#
# pico_sdk_init()
#
# # Add the Musin library (assuming it's in ../musin)
# add_subdirectory(../musin musin_build)
#
# add_executable(my_musin_app main.cpp)
#
# # Link required Musin components and SDK libraries
# target_link_libraries(my_musin_app PRIVATE
#   pico_stdlib
#   musin::core
#   musin::hal
#   musin::drivers
#   musin::ui
#   # Add other musin::* targets as needed
# )
#
# # If using WS2812 driver, generate PIO header for the *application*
# pico_generate_pio_header(my_musin_app ${CMAKE_SOURCE_DIR}/../musin/drivers/ws2812.pio)
#
# # If using filesystem, enable it for the *application*
# # pico_enable_filesystem(my_musin_app)
#
# pico_add_extra_outputs(my_musin_app)
#

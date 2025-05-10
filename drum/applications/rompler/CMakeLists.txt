cmake_minimum_required(VERSION 3.13...3.27)


add_executable(rompler
  main.cpp
)

musin_init(rompler)

target_include_directories(rompler PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../..
)


pico_enable_stdio_usb(rompler 1)
pico_add_extra_outputs(rompler)

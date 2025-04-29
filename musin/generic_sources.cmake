# Put any sources which are not platform dependent here, mainly
# so that they can be easily included in tests.
# This can be structured better if/when the pico port is properly broken out,
# but should be okay for now.

set(MUSIN_ROOT ${CMAKE_CURRENT_LIST_DIR})
set(MUSIN_AUDIO ${MUSIN_ROOT}/audio)
set(MUSIN_UI ${MUSIN_ROOT}/ui)
set(MUSIN_LIBRARIES ${MUSIN_ROOT}/ports/pico/libraries)
set(MUSIN_USB ${MUSIN_ROOT}/usb)
set(MUSIN_DRIVERS ${MUSIN_ROOT}/drivers)

set(musin_audio_generic_sources
  ${MUSIN_AUDIO}/data_ulaw.c
  ${MUSIN_AUDIO}/crusher.cpp
  ${MUSIN_AUDIO}/waveshaper.cpp
  ${MUSIN_AUDIO}/filter.cpp
)

# Put any sources which are not platform dependent here, mainly
# so that they can be easily included in tests.
# This can be structured better if/when the pico port is properly broken out,
# but should be okay for now.

include(${CMAKE_CURRENT_LIST_DIR}/paths.cmake)

set(musin_audio_generic_sources
  ${MUSIN_AUDIO}/data_ulaw.c
  ${MUSIN_AUDIO}/crusher.cpp
  ${MUSIN_AUDIO}/waveshaper.cpp
  ${MUSIN_AUDIO}/filter.cpp
)

#include <catch2/catch_test_macros.hpp>

#include "AudioSampleKick.h"
#include "audio_memory_reader.h"
#include "pitch_shifter.h"

TEST_CASE("PitchShifter reads kick samples") {
  PitchShifter<AudioMemoryReader> shifter;

  shifter.init(AudioSampleKick, AudioSampleKickSize);

  auto count = 0;
  int16_t buffer[256];
  shifter.set_speed(1.5);

  while (shifter.has_data()) {
    shifter.read_samples(buffer, 256);
    ++count;
  }
  REQUIRE(count == (int)(40 / 1.5));
}

#ifndef SOUND_H_2P4SDIWG
#define SOUND_H_2P4SDIWG

#include "buffer_source.h"
#include "pitch_shifter.h"
#include <pico/stdlib.h>

struct Sound : BufferSource {
  constexpr Sound(SampleReader &reader) : pitch_shifter(reader) {
  }

  constexpr void play(const double speed) {
    // printf("Playing drum\n");
    pitch_shifter.set_speed(speed);
    pitch_shifter.reset();
  }

  constexpr void __not_in_flash_func(fill_buffer)(AudioBlock &out_samples) {
    const uint32_t count = pitch_shifter.read_samples(out_samples);
    for (size_t i = count; i < out_samples.size(); i++) {
      out_samples[i] = 0;
    }
  }

  PitchShifter<HardwareLinearInterpolator> pitch_shifter;
};

#endif /* end of include guard: SOUND_H_2P4SDIWG */

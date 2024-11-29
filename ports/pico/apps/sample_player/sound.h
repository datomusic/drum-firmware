#ifndef SOUND_H_2P4SDIWG
#define SOUND_H_2P4SDIWG

#include "buffer_source.h"
#include "timestretched/audio_memory_reader.h"
#include "timestretched/pitch_shifter.h"
#include <stdint.h>
#include <stdio.h>

struct Sound : BufferSource {
  Sound(const unsigned int *sample_data, const size_t data_length)
      : memory_reader(sample_data, data_length), pitch_shifter(memory_reader) {
  }

  void play(const double speed) {
    printf("Playing drum\n");
    pitch_shifter.set_speed(speed);
    pitch_shifter.reset();
  }

  uint32_t fill_buffer(int16_t *out_samples) {
    // printf("Max samples: %i\n", out_buffer->max_sample_count);
    if (pitch_shifter.has_data()) {
      return pitch_shifter.read_samples(out_samples);
    } else {
      printf("Filling empty buffer\n");
      for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
        out_samples[i] = 0; // L
      }

      return AUDIO_BLOCK_SAMPLES;
    }
  }

  AudioMemoryReader memory_reader;
  PitchShifter pitch_shifter;
};

#endif /* end of include guard: SOUND_H_2P4SDIWG */

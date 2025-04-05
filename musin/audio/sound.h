#ifndef SOUND_H_2P4SDIWG
#define SOUND_H_2P4SDIWG

#include "buffer_source.h"
#include "audio_memory_reader.h"
#include "pitch_shifter.h"
#include <pico/stdlib.h>
#include <stdint.h>

struct Sound : BufferSource {
  Sound(const unsigned int *sample_data, const uint32_t data_length)
      : memory_reader(sample_data, data_length), pitch_shifter(memory_reader) {
  }

  void play(const double speed) {
    // printf("Playing drum\n");
    pitch_shifter.set_speed(speed);
    pitch_shifter.reset();
  }

  uint32_t __not_in_flash_func(fill_buffer)(AudioBlock& out_samples) {
    // printf("Max samples: %i\n", out_buffer->max_sample_count);
    if (pitch_shifter.has_data()) {
      return pitch_shifter.read_samples(out_samples);
    } else {
      // printf("Filling empty buffer\n");
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

#ifndef SOUND_H_2P4SDIWG
#define SOUND_H_2P4SDIWG

#include "buffer_source.h"
#include "timestretched/audio_memory_reader.h"
#include "timestretched/pitch_shifter.h"
#include <stdint.h>
#include <stdio.h>

struct Sound : BufferSource {
  // static double playback_speed = 1;

  Sound(const unsigned int *sample_data, const size_t data_length)
      : memory_reader(sample_data, data_length), pitch_shifter(memory_reader) {
  }

  void play() {
    printf("Playing drum\n");
    // pitch_shifter.set_speed(playback_speed);
    pitch_shifter.reset();
  }

  uint32_t fill_buffer(int16_t *out_samples) {
    // printf("Max samples: %i\n", out_buffer->max_sample_count);
    if (pitch_shifter.has_data()) {
      int16_t source_buffer[AUDIO_BLOCK_SAMPLES];
      const uint32_t read_count = pitch_shifter.read_samples(source_buffer);
      // printf("Read %i samples, max: %i\n", read_count,
      // out_buffer->max_sample_count);
      for (uint i = 0; i < read_count; i++) {
        int32_t sample = source_buffer[i];
        // use 32bit full scale
        sample = sample + (sample >> 16u);
        out_samples[i * 2 + 0] = sample; // L
        out_samples[i * 2 + 1] = sample; // R
      }
      return read_count;
    } else {
      printf("Filling empty buffer\n");
      for (uint i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
        out_samples[i * 2 + 0] = 0; // L
        out_samples[i * 2 + 1] = 0; // R
      }

      return AUDIO_BLOCK_SAMPLES;
    }
  }

  AudioMemoryReader memory_reader;
  PitchShifter pitch_shifter;
};

#endif /* end of include guard: SOUND_H_2P4SDIWG */

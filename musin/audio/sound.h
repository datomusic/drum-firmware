#ifndef SOUND_H_2P4SDIWG
#define SOUND_H_2P4SDIWG

#include "audio_memory_reader.h"
#include "buffer_source.h"
#include "pitch_shifter.h"
#include <pico/stdlib.h>
#include <stdint.h>

struct Sound : BufferSource {
  Sound(const unsigned int *sample_data, const uint32_t data_length)
      : memory_reader(sample_data, data_length), pitch_shifter(memory_reader) {
    // No explicit initialization needed here now.
  }

  /**
   * @brief Triggers playback from the beginning at the specified speed.
   * @param speed The playback speed multiplier (1.0 = original pitch).
   */
  void play(float speed) {
    pitch_shifter.set_speed(speed);
    pitch_shifter.reset();
  }

  // Remove the separate pitch() method

  void __not_in_flash_func(fill_buffer)(AudioBlock &out_samples) {
    // printf("Max samples: %i\n", out_buffer->max_sample_count);
    if (pitch_shifter.has_data()) {
      pitch_shifter.read_samples(out_samples);
    } else {
      // printf("Filling empty buffer\n");
      for (size_t i = 0; i < out_samples.size(); i++) {
        out_samples[i] = 0; // L
      }
    }
  }

  AudioMemoryReader memory_reader;
  PitchShifter pitch_shifter;
};

#endif /* end of include guard: SOUND_H_2P4SDIWG */

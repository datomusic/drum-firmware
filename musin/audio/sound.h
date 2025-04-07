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
    // Initialize with default pitch (speed = 1.0)
    pitch_shifter.set_speed(1.0);
  }

  /**
   * @brief Triggers playback from the beginning using the currently set pitch.
   */
  void play() {
    pitch_shifter.reset();
  }

  /**
   * @brief Sets the playback pitch/speed using a normalized value.
   * Maps [0.0, 1.0] to a suitable playback speed range (e.g., [0.1, 4.0]).
   * @param pitch_normalized Value between 0.0 and 1.0. Clamped internally.
   */
  void pitch(float pitch_normalized) {
      float clamped_pitch = etl::clamp(pitch_normalized, 0.0f, 1.0f);
      // Example mapping: 0.0 -> 0.1 speed, 1.0 -> 4.0 speed
      const float min_speed = 0.1f;
      const float max_speed = 4.0f;
      // Could use linear or logarithmic mapping here. Linear is simpler:
      const float speed = min_speed + clamped_pitch * (max_speed - min_speed);
      pitch_shifter.set_speed(speed);
  }


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

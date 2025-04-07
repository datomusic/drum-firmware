#ifndef FILE_SOUND_H_QZUFVAE4
#define FILE_SOUND_H_QZUFVAE4

#include "musin/audio/buffer_source.h"
#include "musin/audio/pitch_shifter.h"
#include <pico/stdlib.h>
#include <stdint.h>

struct FileSound : BufferSource {
  FileSound() : pitch_shifter(reader) {
  }

  void play(const double speed) {
    // printf("Playing drum\n");
    pitch_shifter.set_speed(speed);
    pitch_shifter.reset();
  }

  void load(const char *file_name) {
    reader.load(file_name);
  }

  uint32_t __not_in_flash_func(fill_buffer)(int16_t *out_samples) {
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

  FileReader reader;
  PitchShifter pitch_shifter;
};

#endif /* end of include guard: FILE_SOUND_H_QZUFVAE4 */

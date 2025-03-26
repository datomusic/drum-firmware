#ifndef FILE_SOUND_H_QZUFVAE4
#define FILE_SOUND_H_QZUFVAE4

#include "musin/audio/audio_memory_reader.h"
#include "musin/audio/buffer_source.h"
#include "musin/audio/pitch_shifter.h"
#include "musin/audio/sample_reader.h"
#include <pico/stdlib.h>
#include <stdint.h>
#include <stdio.h>

struct FileReader : SampleReader {
  FileReader() : data_available(false) {
  }

  void load(const char *file_name) {
    this->file_name = file_name;
    this->read_count = 0;
    handle = fopen(file_name, "rb");
    if (!handle) {
      /*printf("[FileReader] Failed opening sample!\n");*/
    } else {
      data_available = false;
      needs_update = false;
      /*printf("[FileReader] Loaded sample!\n");*/
      update();
    }
  }

  void update() {
    this->needs_update = false;
    if (!data_available) {
      return;
    }

    // TODO: Convert from source format to int16_t
    read_count = fread(buffer, sizeof(int16_t), AUDIO_BLOCK_SAMPLES, handle);
    // printf("read_count: %i\n", read_count);
    if (read_count < AUDIO_BLOCK_SAMPLES) {
      data_available = false;
    }
  }

  void reset() {
    if (handle) {
      fclose(handle);
      handle = fopen(file_name, "rb");
      update();
    }

    data_available = handle != 0;
    /*printf("data_available: %i\n", data_available);*/
  }

  bool has_data() {
    return data_available;
  }

  uint32_t __not_in_flash_func(read_samples)(int16_t *out) {
    // printf("[FileReader] read samples\n");
    this->needs_update = true;
    if (read_count > 0) {
      for (size_t i = 0; i < read_count; ++i) {
        out[i] = buffer[i];
      }

      return read_count;
    } else {
      return 0;
    }
  }

  bool needs_update;

private:
  const char *file_name;
  size_t read_count = 0;
  FILE *handle;
  bool data_available;
  int16_t buffer[AUDIO_BLOCK_SAMPLES];
};

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

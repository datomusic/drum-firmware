#ifndef FILE_READER_H_QUO8VKTG
#define FILE_READER_H_QUO8VKTG

#include "musin/audio/sample_reader.h"
#include <pico/stdlib.h>

// TODO: namespace should not be capitalized...
namespace Musin::Audio {

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

  uint32_t __not_in_flash_func(read_samples)(AudioBlock &out) {
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

} // namespace Musin::Audio

#endif /* end of include guard: FILE_READER_H_QUO8VKTG */

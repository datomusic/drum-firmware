#ifndef FILE_READER_H_QUO8VKTG
#define FILE_READER_H_QUO8VKTG

#include "musin/audio/sample_reader.h"
#include <pico/stdlib.h>

// TODO: namespace should not be capitalized...
namespace musin::Audio {

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
      update_needed = false;
      /*printf("[FileReader] Loaded sample!\n");*/
      update();
    }
  }

  bool needs_update() const {
    return update_needed;
  }

  void update() {
    if (!update_needed || !data_available) {
      return;
    }

    this->update_needed = false;

    // TODO: Convert from source format to int16_t
    read_count = fread(buffer, sizeof(int16_t), AudioBlock::MaxSamples, handle);
    // printf("read_count: %i\n", read_count);
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
    if (!has_data()) {
      return 0;
    }

    // printf("[FileReader] read samples\n");
    this->update_needed = true;
    if (read_count > 0) {
      for (size_t i = 0; i < read_count; ++i) {
        out[i] = buffer[i];
      }

      if (read_count < out.size()) {
        data_available = false;
      }

      return read_count;
    } else {
      return 0;
    }
  }

private:
  const char *file_name;
  size_t read_count = 0;
  FILE *handle;
  bool data_available;
  int16_t buffer[AudioBlock::MaxSamples];
  bool update_needed = false;
};

} // namespace musin::Audio

#endif /* end of include guard: FILE_READER_H_QUO8VKTG */

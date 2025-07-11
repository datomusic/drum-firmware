#ifndef FILE_READER_H_QUO8VKTG
#define FILE_READER_H_QUO8VKTG

#include "musin/audio/sample_reader.h"
#include <pico/stdlib.h>

// TODO: namespace should not be capitalized...
namespace musin::Audio {

struct FileReader : SampleReader {
  FileReader()
      : file_name(nullptr), read_count(0), handle(nullptr),
        data_available(false), update_needed(false), current_idx_in_buffer(0) {
  }

  void load(const char *new_file_name) {
    if (handle) { // Close previous file if any
      fclose(handle);
      handle = nullptr;
    }
    this->file_name = new_file_name;
    this->read_count = 0;
    this->current_idx_in_buffer = 0;
    this->update_needed = false; // Reset update_needed flag

    if (!this->file_name) {
      this->data_available = false;
      return;
    }

    handle = fopen(this->file_name, "rb");
    if (!handle) {
      /*printf("[FileReader] Failed opening sample: %s\n", this->file_name);*/
      this->data_available = false;
    } else {
      /*printf("[FileReader] Loaded sample: %s\n", this->file_name);*/
      this->data_available = true; // File is open, data might be available
      // Buffer is empty, first read_next or has_data check will trigger a fill
      // if needed.
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
    // Uses the existing file_name. Re-loads it.
    if (this->file_name) {
      load(this->file_name); // load handles closing previous handle, reopening,
                             // and setting state
    } else {
      // No file name was ever set, or it was null.
      if (handle) { // Should not happen if file_name is null, but defensive
        fclose(handle);
        handle = nullptr;
      }
      this->data_available = false;
      this->read_count = 0;
      this->current_idx_in_buffer = 0;
      this->update_needed = false;
    }
  }

  // This method overrides a pure virtual method from SampleReader
  bool has_data() override {
    // True if there are samples ready in the current buffer,
    // OR if the file stream is still marked as available (this->data_available
    // = true) meaning a read attempt might yield more samples.
    return (current_idx_in_buffer < read_count) || this->data_available;
  }

  bool read_next(int16_t &out) override {
    if (current_idx_in_buffer >= read_count) { // Internal buffer is exhausted
      if (!this->data_available) { // And file stream is known to be exhausted
        return false;
      }

      // Attempt to read from file
      if (!handle) {
        // This case implies data_available was true but handle is null.
        // This shouldn't happen if logic is correct. Mark as no data.
        this->data_available = false;
        return false;
      }

      this->read_count =
          fread(this->buffer, sizeof(int16_t), AudioBlock::MaxSamples, handle);
      this->current_idx_in_buffer = 0;

      if (this->read_count == 0) { // No samples were read, means EOF or error.
        this->data_available =
            false; // No more data expected from the file stream.
        // Consider closing the file handle here to release resources if it's
        // truly EOF. fclose(handle); handle = nullptr; // This might affect
        // reset() behavior.
        return false;
      }
      // If read_count < AudioBlock::MaxSamples but > 0, some data was read,
      // but it's the last block from the file.
      // Mark data_available as false so that after this block is consumed,
      // has_data() will return false (unless reset).
      if (this->read_count < AudioBlock::MaxSamples) {
        this->data_available = false;
      }
    }

    out = this->buffer[this->current_idx_in_buffer++];
    return true;
  }

  uint32_t __not_in_flash_func(read_samples)(AudioBlock &out) override {
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
  const char *file_name; // Initialized in constructor
  size_t read_count;     // Initialized in constructor
  FILE *handle;          // Initialized in constructor
  bool data_available;   // Initialized in constructor
  int16_t buffer[AudioBlock::MaxSamples];
  bool update_needed;           // Initialized in constructor
  size_t current_idx_in_buffer; // Initialized in constructor
};

} // namespace musin::Audio

#endif /* end of include guard: FILE_READER_H_QUO8VKTG */

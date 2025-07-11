#include "musin/audio/unbuffered_file_sample_reader.h"

#include "musin/audio/block.h" // For AudioBlock
#include <algorithm>           // For std::fill

namespace musin {

UnbufferedFileSampleReader::UnbufferedFileSampleReader() = default;

UnbufferedFileSampleReader::~UnbufferedFileSampleReader() {
  close();
}

bool UnbufferedFileSampleReader::open(const etl::string_view &path) {
  close(); // Ensure any previously opened file is closed.

  file_handle_ = fopen(path.data(), "rb");
  if (!file_handle_) {
    // Optional: Add logging for failed file open.
    // printf("Failed to open file: %s\n", path.data());
    return false;
  }
  return true;
}

void UnbufferedFileSampleReader::close() {
  if (file_handle_) {
    fclose(file_handle_);
    file_handle_ = nullptr;
  }
}

void UnbufferedFileSampleReader::reset() {
  if (file_handle_) {
    // Setting read position to the beginning of the file
    fseek(file_handle_, 0, SEEK_SET);
  }
}

bool UnbufferedFileSampleReader::has_data() {
  if (!file_handle_) {
    return false;
  }
  return !feof(file_handle_) && !ferror(file_handle_);
}

uint32_t UnbufferedFileSampleReader::read_samples(AudioBlock &out) {
  if (!has_data()) {
    std::fill(out.begin(), out.end(), 0);
    return 0;
  }

  const size_t samples_read =
      fread(out.begin(), sizeof(int16_t), out.size(), file_handle_);

  if (samples_read < out.size()) {
    // Fill the rest of the buffer with silence if we hit EOF.
    std::fill(out.begin() + samples_read, out.end(), 0);
  }

  return samples_read;
}

bool UnbufferedFileSampleReader::read_next(int16_t &out) {
  if (!has_data()) {
    return false;
  }

  const size_t samples_read = fread(&out, sizeof(int16_t), 1, file_handle_);
  return samples_read == 1;
}

} // namespace musin

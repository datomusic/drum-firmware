#ifndef CHUNK_READER_H_FBMGJA3O
#define CHUNK_READER_H_FBMGJA3O
#include "sample_reader.h"
#include <stdint.h>

template <int BUFFER_SIZE> struct ChunkReader {
  ChunkReader(SampleReader &reader) : reader(reader) {};

  void reset() {
    reader.reset();
    bytes_read = 0;
    read_position = 0;
  }

  // Reader interface
  bool has_data() {
    return (read_position < bytes_read) || reader.has_data();
  }

  // Reader interface
  bool read_next(int16_t *out) {
    if (read_position >= bytes_read) {
      bytes_read = reader.read_samples(buffer, BUFFER_SIZE);
      read_position = 0;
    }

    if (read_position < bytes_read) {
      *out = buffer[read_position];
      read_position++;
      return true;
    } else {
      *out = 0;
      return false;
    }
  }

private:
  SampleReader &reader;
  int16_t buffer[BUFFER_SIZE];
  uint32_t bytes_read = 0;
  uint32_t read_position = 0;
};

#endif /* end of include guard: CHUNK_READER_H_FBMGJA3O */

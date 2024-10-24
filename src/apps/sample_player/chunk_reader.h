#ifndef CHUNK_READER_H_FBMGJA3O
#define CHUNK_READER_H_FBMGJA3O

#include "sample_reader.h"

template <int BUFFER_SIZE> struct ChunkReader : SampleReader {
  ChunkReader(SampleReader &reader) : reader(reader) {};

  // Reader interface
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
  uint32_t read_samples(int16_t *out, const uint16_t output_sample_count) {
    uint32_t written_samples = 0;
    for (int sample_index = 0; sample_index < output_sample_count;
         ++sample_index) {
      if (read_position < bytes_read) {
        out[sample_index] = buffer[read_position];
        read_position++;
        written_samples++;
      } else {
        bytes_read = reader.read_samples(buffer, BUFFER_SIZE);
        read_position = 0;
        if (bytes_read > 0) {
          out[sample_index] = buffer[read_position];
          read_position++;
          written_samples++;
        } else {
          break;
        }
      }
    }

    return written_samples;
  }

private:
  SampleReader &reader;
  int16_t buffer[BUFFER_SIZE];
  uint32_t bytes_read = 0;
  uint32_t read_position = 0;
};

#endif /* end of include guard: CHUNK_READER_H_FBMGJA3O */

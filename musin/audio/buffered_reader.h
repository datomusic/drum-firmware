#ifndef CHUNK_READER_H_FBMGJA3O
#define CHUNK_READER_H_FBMGJA3O

#include "etl/array.h"
#include "sample_reader.h" // Includes block.h for AudioBlock and AUDIO_BLOCK_SAMPLES
#include <algorithm>       // For std::copy
#include <stdint.h>

// Default number of audio blocks to buffer in RAM.
// Each block is AUDIO_BLOCK_SAMPLES (e.g., 128) samples.
// Total RAM = NumRamBlocks * AUDIO_BLOCK_SAMPLES * sizeof(int16_t).
// E.g., 2 blocks * 128 samples/block * 2 bytes/sample = 512 bytes.
constexpr size_t DEFAULT_BUFFERED_READER_RAM_BLOCKS = 4;

template <size_t NumRamBlocks = DEFAULT_BUFFERED_READER_RAM_BLOCKS> struct BufferedReader {
  static_assert(NumRamBlocks > 0, "Number of RAM blocks must be greater than 0");
  static constexpr size_t TOTAL_RAM_BUFFER_SAMPLES = NumRamBlocks * AUDIO_BLOCK_SAMPLES;

  constexpr BufferedReader(SampleReader &reader) : reader(reader) {};

  constexpr void reset() {
    reader.reset();
    samples_currently_in_buffer = 0;
    current_read_position = 0;
  }

  constexpr bool has_data() const {
    return (current_read_position < samples_currently_in_buffer) || reader.has_data();
  }

  constexpr bool read_next(int16_t &out) {
    if (current_read_position >= samples_currently_in_buffer) {
      samples_currently_in_buffer = 0;
      current_read_position = 0;

      for (size_t block_fill_idx = 0; block_fill_idx < NumRamBlocks; ++block_fill_idx) {
        if (!reader.has_data()) {
          break;
        }

        AudioBlock temp_block_for_source_read;
        uint32_t samples_fetched_this_iteration = reader.read_samples(temp_block_for_source_read);

        if (samples_fetched_this_iteration == 0) {
          break;
        }

        int16_t *destination_start_ptr = internal_ram_buffer.begin() + samples_currently_in_buffer;
        std::copy(temp_block_for_source_read.begin(),
                  temp_block_for_source_read.begin() + samples_fetched_this_iteration,
                  destination_start_ptr);

        samples_currently_in_buffer += samples_fetched_this_iteration;
      }
    }

    if (current_read_position < samples_currently_in_buffer) {
      out = internal_ram_buffer[current_read_position];
      current_read_position++;
      return true;
    } else {
      return false;
    }
  }

private:
  SampleReader &reader;
  etl::array<int16_t, TOTAL_RAM_BUFFER_SAMPLES> internal_ram_buffer;
  uint32_t samples_currently_in_buffer = 0;
  uint32_t current_read_position = 0;
};

#endif /* end of include guard: CHUNK_READER_H_FBMGJA3O */

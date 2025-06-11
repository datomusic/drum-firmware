#ifndef CHUNK_READER_H_FBMGJA3O
#define CHUNK_READER_H_FBMGJA3O

#include "etl/array.h"
#include "musin/audio/cpu_copier.h"
#include "sample_reader.h" // Includes block.h for AudioBlock and AUDIO_BLOCK_SAMPLES
#include <algorithm>       // For std::copy
#include <stdint.h>

namespace musin {

// Default number of audio blocks for EACH of the two internal ping-pong buffers.
// Each block is AUDIO_BLOCK_SAMPLES (e.g., 128) samples.
// Total RAM = 2 * NumBlocksPerSlot * AUDIO_BLOCK_SAMPLES * sizeof(int16_t).
// E.g., 2 slots * 4 blocks/slot * 128 samples/block * 2 bytes/sample = 2048 bytes.
constexpr size_t DEFAULT_AUDIO_BLOCKS_PER_BUFFER_SLOT = 1;

template <size_t NumBlocksPerSlot = DEFAULT_AUDIO_BLOCKS_PER_BUFFER_SLOT,
          typename CopierPolicy = CpuCopier>
struct BufferedReader {
  static_assert(NumBlocksPerSlot > 0, "Number of RAM blocks per slot must be greater than 0");
  static constexpr size_t SAMPLES_PER_SLOT = NumBlocksPerSlot * AUDIO_BLOCK_SAMPLES;

  constexpr BufferedReader(SampleReader &reader)
      : reader(reader), active_buffer_ptr(&buffer_a), inactive_buffer_ptr(&buffer_b),
        samples_in_active_buffer(0), current_read_position_in_active_buffer(0) {
  }

  constexpr void reset() {
    reader.reset();
    active_buffer_ptr = &buffer_a;
    inactive_buffer_ptr = &buffer_b;
    samples_in_active_buffer = 0;
    current_read_position_in_active_buffer = 0;
    // Optionally, pre-fill the first buffer here, or let read_next handle it.
    // For simplicity, let read_next handle the initial fill.
  }

  constexpr bool has_data() const {
    return (current_read_position_in_active_buffer < samples_in_active_buffer) || reader.has_data();
  }

private:
  constexpr void fill_buffer_slot(etl::array<int16_t, SAMPLES_PER_SLOT> &slot_to_fill,
                                  uint32_t &out_samples_filled) {
    out_samples_filled = 0;
    for (size_t block_fill_idx = 0; block_fill_idx < NumBlocksPerSlot; ++block_fill_idx) {
      if (!reader.has_data()) {
        break;
      }

      AudioBlock temp_block_for_source_read;
      uint32_t samples_fetched_this_iteration = reader.read_samples(temp_block_for_source_read);

      if (samples_fetched_this_iteration == 0) {
        break;
      }

      int16_t *destination_start_ptr = slot_to_fill.begin() + out_samples_filled;
      CopierPolicy::copy(destination_start_ptr, temp_block_for_source_read.begin(),
                         samples_fetched_this_iteration);
      out_samples_filled += samples_fetched_this_iteration;
    }
  }

public:
  // Reads a chunk of samples into the provided destination buffer.
  // Returns the number of samples actually read.
  constexpr uint32_t read_buffered_chunk(int16_t *dest_buffer, uint32_t samples_requested) {
    uint32_t samples_copied_total = 0;

    while (samples_copied_total < samples_requested) {
      if (current_read_position_in_active_buffer >= samples_in_active_buffer) {
        // Active buffer is exhausted, switch and attempt to fill the new active one.
        etl::array<int16_t, SAMPLES_PER_SLOT> *temp_ptr = active_buffer_ptr;
        active_buffer_ptr = inactive_buffer_ptr;
        inactive_buffer_ptr = temp_ptr;

        current_read_position_in_active_buffer = 0;
        samples_in_active_buffer = 0; // Mark as empty before fill attempt

        fill_buffer_slot(*active_buffer_ptr, samples_in_active_buffer);

        if (samples_in_active_buffer == 0) {
          // No more data could be buffered.
          break;
        }
      }

      uint32_t samples_to_copy_this_iteration =
          std::min(samples_requested - samples_copied_total,
                   samples_in_active_buffer - current_read_position_in_active_buffer);

      std::copy(active_buffer_ptr->begin() + current_read_position_in_active_buffer,
                active_buffer_ptr->begin() + current_read_position_in_active_buffer +
                    samples_to_copy_this_iteration,
                dest_buffer + samples_copied_total);

      current_read_position_in_active_buffer += samples_to_copy_this_iteration;
      samples_copied_total += samples_to_copy_this_iteration;
    }
    return samples_copied_total;
  }

  constexpr bool read_next(int16_t &out) {
    if (current_read_position_in_active_buffer >= samples_in_active_buffer) {
      // Active buffer is exhausted, switch and attempt to fill the new active one.
      etl::array<int16_t, SAMPLES_PER_SLOT> *temp_ptr = active_buffer_ptr;
      active_buffer_ptr = inactive_buffer_ptr;
      inactive_buffer_ptr = temp_ptr;

      current_read_position_in_active_buffer = 0;
      samples_in_active_buffer = 0; // Mark as empty before fill attempt

      fill_buffer_slot(*active_buffer_ptr, samples_in_active_buffer);
    }

    if (current_read_position_in_active_buffer < samples_in_active_buffer) {
      out = (*active_buffer_ptr)[current_read_position_in_active_buffer];
      current_read_position_in_active_buffer++;
      return true;
    } else {
      // Current buffer is (still) empty and could not be refilled sufficiently.
      return false;
    }
  }

private:
  SampleReader &reader;
  etl::array<int16_t, SAMPLES_PER_SLOT> buffer_a;
  etl::array<int16_t, SAMPLES_PER_SLOT> buffer_b;

  etl::array<int16_t, SAMPLES_PER_SLOT> *active_buffer_ptr;
  etl::array<int16_t, SAMPLES_PER_SLOT> *inactive_buffer_ptr;

  uint32_t samples_in_active_buffer;
  uint32_t current_read_position_in_active_buffer;
};

} // namespace musin

#endif /* end of include guard: CHUNK_READER_H_FBMGJA3O */

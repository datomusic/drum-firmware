#ifndef PCM_READER_H_GB952ZMC
#define PCM_READER_H_GB952ZMC

#include "sample_reader.h"
#include <algorithm> // For std::copy

namespace musin {

// Streams items of DataType in chunks, into some iterator.
// The user must ensure the iterator can accept max ChunkSize items.
template <typename DataType, typename OutputIterator, int ChunkSize>
struct MemoryReader {
  // Add default arguments to allow default construction
  constexpr MemoryReader(const DataType *items = nullptr,
                         const uint32_t count = 0) {
    set_source(items, count);
  }

  constexpr void set_source(const DataType *items, const uint32_t count) {
    this->items = items;
    this->read_pos = 0;
    this->count = count;
  }

  constexpr void reset() {
    read_pos = 0;
  }

  constexpr bool has_data() {
    return read_pos < count;
  }

  constexpr uint32_t read_chunk(OutputIterator output) {
    const DataType *const iterator = items + read_pos;
    const uint32_t rest = count - read_pos;
    uint32_t read_count = ChunkSize;

    if (rest < ChunkSize) {
      read_count = rest;
    }

    read_pos += read_count;

    const DataType *const last = iterator + read_count;
    std::copy(iterator, last, output);
    return read_count;
  }

private:
  const DataType *items;
  uint32_t read_pos;
  uint32_t count;
};

// Specialization of MemoryReader for 16bit samples, implementing the
// SampleReader interface
struct MemorySampleReader : SampleReader {
  // Provide a default constructor for etl::optional
  constexpr MemorySampleReader()
      : reader(nullptr, 0), m_buffer_read_idx(0), m_buffer_valid_samples(0) {
  }

  constexpr MemorySampleReader(const int16_t *items, const uint32_t count)
      : reader(items, count), m_buffer_read_idx(0), m_buffer_valid_samples(0) {
  }

  constexpr void set_source(const int16_t *items, const uint32_t count) {
    reader.set_source(items, count);
    m_buffer_read_idx = 0;
    m_buffer_valid_samples = 0;
  }

  // Reader interface
  constexpr void reset() override {
    reader.reset();
    m_buffer_read_idx = 0;
    m_buffer_valid_samples = 0;
  }

  // Reader interface
  constexpr bool has_data() override {
    return (m_buffer_read_idx < m_buffer_valid_samples) || reader.has_data();
  }

  // Reader interface
  constexpr bool read_next(int16_t &out) override {
    if (m_buffer_read_idx >=
        m_buffer_valid_samples) { // If current buffer is exhausted
      m_buffer_valid_samples = reader.read_chunk(m_buffer.begin()); // Refill it
      m_buffer_read_idx = 0;             // Reset read index for new buffer
      if (m_buffer_valid_samples == 0) { // If refill yielded nothing
        out = 0; // Default value, consistent with other readers
        return false;
      }
    }
    out = m_buffer[m_buffer_read_idx++];
    return true;
  }

  // Reader interface
  constexpr uint32_t read_samples(AudioBlock &out) override {
    uint32_t total_samples_copied_from_source = 0;
    int16_t *out_ptr = out.begin();
    uint32_t samples_to_fill_in_block = AUDIO_BLOCK_SAMPLES;

    // 1. Drain from m_buffer
    if (m_buffer_read_idx < m_buffer_valid_samples) {
      uint32_t can_copy_from_m_buffer =
          m_buffer_valid_samples - m_buffer_read_idx;
      uint32_t num_to_copy =
          std::min(can_copy_from_m_buffer, samples_to_fill_in_block);

      std::copy(m_buffer.begin() + m_buffer_read_idx,
                m_buffer.begin() + m_buffer_read_idx + num_to_copy, out_ptr);

      m_buffer_read_idx += num_to_copy;
      out_ptr += num_to_copy;
      total_samples_copied_from_source += num_to_copy;
      samples_to_fill_in_block -= num_to_copy;
    }

    // 2. If more samples are needed, read directly from the underlying reader's
    // next chunk(s)
    //    This part assumes m_buffer is now exhausted or was initially empty.
    //    The 'reader' will provide fresh data.
    if (samples_to_fill_in_block > 0) {
      // Mark internal buffer as fully consumed because 'reader' will advance.
      m_buffer_read_idx = 0;
      m_buffer_valid_samples = 0;

      // Read directly into the output block. read_chunk will fill up to
      // AUDIO_BLOCK_SAMPLES or fewer if the source is exhausted.
      uint32_t samples_read_from_main_reader = reader.read_chunk(out_ptr);

      total_samples_copied_from_source += samples_read_from_main_reader;
      out_ptr += samples_read_from_main_reader;
      samples_to_fill_in_block -= samples_read_from_main_reader;
    }

    // 3. Zero-fill the rest of the block if necessary
    if (samples_to_fill_in_block > 0) {
      std::fill(out_ptr, out_ptr + samples_to_fill_in_block, 0);
    }

    return total_samples_copied_from_source;
  }

private:
  MemoryReader<int16_t, int16_t *, AUDIO_BLOCK_SAMPLES> reader;
  AudioBlock m_buffer; // Internal buffer for read_next and to ensure coherence
                       // with read_samples
  uint32_t m_buffer_read_idx;
  uint32_t m_buffer_valid_samples;
};

} // namespace musin

#endif /* end of include guard: PCM_READER_H_GB952ZMC */

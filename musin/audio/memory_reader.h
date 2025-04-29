#ifndef PCM_READER_H_GB952ZMC
#define PCM_READER_H_GB952ZMC

#include "sample_reader.h"

// Streams items of DataType in chunks, into some iterator.
// The user must ensure the iterator can accept max ChunkSize items.
template <typename DataType, typename OutputIterator, int ChunkSize> struct MemoryReader {
  constexpr MemoryReader(const DataType *items, const uint32_t count) {
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

  constexpr uint32_t stream_items(OutputIterator output) {
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

// Specializtion of MemoryReader for 16bit samples, implementing the SampleReader interface
struct MemorySampleReader : SampleReader {
  constexpr MemorySampleReader(const int16_t *items, const uint32_t count) : reader(items, count) {
  }

  constexpr void set_source(const int16_t *items, const uint32_t count) {
    reader.set_source(items, count);
  }

  // Reader interface
  constexpr void reset() {
    reader.reset();
  }

  // Reader interface
  constexpr bool has_data() {
    return reader.has_data();
  }

  // Reader interface
  constexpr uint32_t read_samples(AudioBlock &out) {
    return reader.stream_items(out.begin());
  }

private:
  MemoryReader<int16_t, int16_t *, AUDIO_BLOCK_SAMPLES> reader;
};

#endif /* end of include guard: PCM_READER_H_GB952ZMC */

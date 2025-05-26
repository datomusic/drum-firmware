#ifndef SYSEX_CHUNK_H_JY7GCIDU
#define SYSEX_CHUNK_H_JY7GCIDU

#include <algorithm>

#include "etl/array.h"
#include "musin/midi/midi_wrapper.h"

// NOTE: If `count` is set larger than Data::SIZE, it will be truncated.
//       TODO: Error instead, when `count` is too large.
namespace sysex {
struct Chunk {
  typedef etl::array<uint8_t, MIDI::SysExMaxSize> Data;

  // Copy bytes to make sure we have ownership.
  constexpr Chunk(const uint8_t *bytes, const uint8_t count)
      : data(copy_bytes(bytes, count)), count(count) {
  }

  constexpr const uint8_t &operator[](const size_t i) const {
    // TODO: Add some kind of range checking, at least in debug?
    return data[i];
  }

  constexpr size_t size() const {
    return count;
  }

  constexpr Data::const_iterator cbegin() const {
    return data.cbegin();
  }

  constexpr Data::const_iterator cend() const {
    return data.cbegin() + count;
  }

private:
  const Data data;
  const size_t count;

  static constexpr Data copy_bytes(const uint8_t *bytes, const size_t count) {
    Data copied;
    std::copy(bytes, bytes + std::min(count, Data::SIZE), copied.begin());
    return copied;
  }
};
} // namespace sysex

#endif /* end of include guard: SYSEX_CHUNK_H_JY7GCIDU */

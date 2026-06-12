#ifndef MUSIN_MIDI_SYSEX_CHUNK_H
#define MUSIN_MIDI_SYSEX_CHUNK_H

#include "etl/array.h"
#include "etl/span.h"
#include <cstddef>
#include <cstdint>

namespace sysex {

// Largest SysEx payload (excluding 0xF0/0xF7 framing) that can be received.
// Matches MIDI::SysExMaxSize (2048) minus the two framing bytes.
inline constexpr size_t MAX_PAYLOAD_SIZE = 2046;

/**
 * @class Chunk
 * @brief An owning, fixed-capacity copy of a SysEx message chunk.
 *
 * Copies the payload at construction so the chunk remains valid after the
 * MIDI driver's receive buffer is reused. Payloads longer than
 * MAX_PAYLOAD_SIZE are truncated.
 */
class Chunk {
public:
  using const_iterator = const uint8_t *;

  /**
   * @brief Constructs a Chunk by copying from a raw pointer and size.
   */
  constexpr Chunk(const uint8_t *data, size_t size)
      : size_(size < MAX_PAYLOAD_SIZE ? size : MAX_PAYLOAD_SIZE) {
    for (size_t i = 0; i < size_; ++i) {
      data_[i] = data[i];
    }
  }

  /**
   * @brief Constructs a Chunk by copying from an etl::span.
   */
  explicit constexpr Chunk(etl::span<const uint8_t> view)
      : Chunk(view.data(), view.size()) {
  }

  constexpr const uint8_t &operator[](size_t i) const {
    return data_[i];
  }
  constexpr size_t size() const {
    return size_;
  }
  constexpr bool empty() const {
    return size_ == 0;
  }
  constexpr const_iterator begin() const {
    return data_.data();
  }
  constexpr const_iterator end() const {
    return data_.data() + size_;
  }
  constexpr const_iterator cbegin() const {
    return begin();
  }
  constexpr const_iterator cend() const {
    return end();
  }

private:
  etl::array<uint8_t, MAX_PAYLOAD_SIZE> data_{};
  size_t size_;
};

} // namespace sysex

#endif // MUSIN_MIDI_SYSEX_CHUNK_H

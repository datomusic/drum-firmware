#ifndef SYSEX_CHUNK_H_JY7GCIDU
#define SYSEX_CHUNK_H_JY7GCIDU

#include "etl/span.h"

namespace sysex {

/**
 * @class Chunk
 * @brief A non-owning view of a SysEx message chunk.
 *
 * This class wraps an etl::span to provide a consistent, non-owning
 * interface to a segment of a SysEx message. It avoids copying data,
 * making it efficient for processing message fragments that are owned
 * by another buffer (e.g., the MIDI driver's receive buffer).
 */
class Chunk {
public:
  using const_iterator = etl::span<const uint8_t>::const_iterator;

  /**
   * @brief Constructs a Chunk from a raw pointer and size.
   */
  constexpr Chunk(const uint8_t *data, size_t size) : view_(data, size) {}

  /**
   * @brief Constructs a Chunk from an etl::span.
   */
  constexpr Chunk(etl::span<const uint8_t> view) : view_(view) {}

  constexpr const uint8_t &operator[](size_t i) const { return view_[i]; }
  constexpr size_t size() const { return view_.size(); }
  constexpr bool empty() const { return view_.empty(); }
  constexpr const_iterator begin() const { return view_.begin(); }
  constexpr const_iterator end() const { return view_.end(); }
  constexpr const_iterator cbegin() const { return view_.cbegin(); }
  constexpr const_iterator cend() const { return view_.cend(); }

private:
  etl::span<const uint8_t> view_;
};

} // namespace sysex

#endif /* end of include guard: SYSEX_CHUNK_H_JY7GCIDU */
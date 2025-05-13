#ifndef SYSEX_PROTOCOL_H_O6CX5YEN
#define SYSEX_PROTOCOL_H_O6CX5YEN

#include <algorithm>

#include "etl/array.h"
#include "musin/midi/midi_wrapper.h"

namespace sysex {

// NOTE: If `length` is set larger than Data::SIZE, it will be truncated.
//       TODO: Error instead, when `length` is too large.
struct Chunk {
  typedef etl::array<uint8_t, MIDI::SysExMaxSize> Data;

  // Copy bytes to make sure we have ownership.
  constexpr Chunk(const uint8_t *bytes, const uint8_t length)
      : data(copy_bytes(bytes, length)), length(length) {
  }

  constexpr const uint8_t &operator[](const size_t i) const {
    // TODO: Add some kind of range checking, at least in debug?
    return data[i];
  }

  constexpr size_t size() const {
    return length;
  }

  constexpr Data::const_iterator cbegin() const {
    return data.cbegin();
  }

  constexpr Data::const_iterator cend() const {
    return data.cbegin() + length;
  }

private:
  const Data data;
  const size_t length;

  static constexpr Data copy_bytes(const uint8_t *bytes, const size_t length) {
    Data copied;
    std::copy(bytes, bytes + std::min(length, Data::SIZE), copied.begin());
    return copied;
  }
};

struct Protocol {
  constexpr void handle_chunk(const Chunk &chunk) {
    if (state == State::Idle) {
      if (chunk.size() >= 3 && chunk[1] == DatoId && chunk[2] == DrumId) {
        state = State::Identified;
        parse_part(chunk.cbegin() + 3, chunk.cend());
      }
    } else {
      parse_part(chunk.cbegin(), chunk.cend());
    }
  }

  enum class State {
    Idle,
    Identified,
    FileTransfer,
  };

  // Mainly here for testing. Don't rely on it for external functionality.
  constexpr State __get_state() {
    return state;
  }

private:
  // TODO: Make externally configurable
  static const uint8_t DatoId = 0x7D; // Manufacturer ID for Dato
  static const uint8_t DrumId = 0x65; // Device ID for DRUM

  enum Tag {
    BeginFileTransfer = 0x08,
    EndFileTransfer = 0x09
  };

  constexpr void parse_part(Chunk::Data::const_iterator iterator,
                            const Chunk::Data::const_iterator end) {
    if (iterator == end) {
      return;
    }

    switch (state) {
    case State::Identified: {
      const auto tag = *iterator;
      iterator++;

      if (tag == BeginFileTransfer) {
        state = State::FileTransfer;
        parse_part(iterator, end);
      }
    } break;

    case State::FileTransfer: {
      // TODO: - Decode sysex messages to bytes
      //       - Pass them to some handler based on type
      //       - Currently only one type of file transfer (samples)
    } break;

    case State::Idle: {
      // TODO: Unexpected situation. Log it somehow.
    } break;
    }
  }

  State state = State::Idle;
};
} // namespace sysex

#endif /* end of include guard: SYSEX_PROTOCOL_H_O6CX5YEN */

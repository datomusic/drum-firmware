#ifndef SYSEX_PROTOCOL_H_O6CX5YEN
#define SYSEX_PROTOCOL_H_O6CX5YEN

#include "./chunk.h"
#include "./codec.h"

namespace sysex {

struct Protocol {
  // TODO: Return informative error on failure
  constexpr bool handle_chunk(const Chunk &chunk) {
    Chunk::Data::const_iterator iterator = chunk.cbegin();
    if (chunk.size() == 0 || (*iterator++) != midi::SystemExclusive) {
      return false;
    }

    if (state == State::Idle) {
      if ((*iterator++) == DatoId && (*iterator++) == DrumId) {
        state = State::Identified;
        parse_part(iterator, chunk.cend());
      }
    } else {
      parse_part(iterator, chunk.cend());
    }

    return true;
  }

  enum class State {
    Idle,
    Identified,
    ByteTransfer,
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
    BeginByteTransfer = 0x10,
    Bytes = 0x11,
    EndByteTransfer = 0x12,
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

      if (tag == BeginByteTransfer) {
        state = State::ByteTransfer;
        parse_part(iterator, end);
      }
    } break;

    case State::ByteTransfer: {
      const auto bytes = codec::decode(iterator, end);
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

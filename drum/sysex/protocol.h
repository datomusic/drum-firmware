#ifndef SYSEX_PROTOCOL_H_O6CX5YEN
#define SYSEX_PROTOCOL_H_O6CX5YEN

#include "./chunk.h"
#include "./codec.h"

namespace sysex {

struct Protocol {
  // TODO: Return informative error on failure.
  // Current return value indicates if the message was accepted at all.
  constexpr bool handle_chunk(const Chunk &chunk) {
    Chunk::Data::const_iterator iterator = chunk.cbegin();

    // Make sure it's for us, and we have space for at least a tag
    if (!(chunk.size() >= 4 && (*iterator++) == 0 && (*iterator++) == DatoId &&
          (*iterator++) == DrumId)) {
      return false;
    }

    etl::array<uint16_t, Chunk::Data::SIZE> bytes;
    const auto byte_count =
        codec::decode<Chunk::Data::SIZE>(etl::span(iterator, chunk.cend()), bytes);

    if (byte_count == 0) {
      return true;
    }

    auto byte_iterator = bytes.cbegin();
    const uint16_t tag = (*byte_iterator++);
    handle_packet(tag, etl::span(byte_iterator, bytes.cend()));

    return true;
  }

  enum class State {
    Idle,
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

  constexpr void handle_packet(const uint16_t tag,
                               const etl::span<const uint16_t, Chunk::Data::SIZE> &bytes) {
    switch (tag) {
    case BeginByteTransfer: {
    } break;
    case Bytes: {
    } break;
    case EndByteTransfer: {
    } break;
    }
  };

  State state = State::Idle;
};
} // namespace sysex

#endif /* end of include guard: SYSEX_PROTOCOL_H_O6CX5YEN */

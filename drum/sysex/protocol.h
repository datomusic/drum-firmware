#ifndef SYSEX_PROTOCOL_H_O6CX5YEN
#define SYSEX_PROTOCOL_H_O6CX5YEN
#include "../file_ops.h"
#include "./chunk.h"
#include "./codec.h"

// TODO: Currently handles file streaming/writing, as well as sysex decoding.
//       Processing of the byte stream can be offloaded to something external,
//       which keeps this focused on the sysex transport layer, while dealing with
//       the data happens elsewhere. That way, we could change transport later or support
//       multiple transports, like WebSerial etc.

namespace sysex {

template <typename FileHandle> struct Protocol {
  typedef FileOps<FileHandle, 128> FileOperations;

  constexpr Protocol(FileOperations file_ops) : file_ops(file_ops) {
  }

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
  const FileOperations file_ops;
  etl::optional<FileHandle> file_handle;
  State state = State::Idle;

  // TODO: Make externally configurable
  static const uint8_t DatoId = 0x7D; // Manufacturer ID for Dato
  static const uint8_t DrumId = 0x65; // Device ID for DRUM

  enum Tag {
    BeginFileWrite = 0x10,
    FileBytes = 0x11,
    EndFileTransfer = 0x12,
  };

  constexpr void handle_packet(const uint16_t tag,
                               const etl::span<const uint16_t, Chunk::Data::SIZE> &bytes) {
    switch (state) {
    case State::ByteTransfer: {
      if (tag == FileBytes) {
        // Stream to open temporary file
      } else {
        // Close the file and report error.
      }
    } break;
    default:
      switch (tag) {
      case BeginFileWrite: {
        // TODO: Error if file is already open, maybe?
        // Get file path
      } break;
      case EndFileTransfer: {
        // TODO: Close the handle
      } break;
      }
    }
  };
};
} // namespace sysex

#endif /* end of include guard: SYSEX_PROTOCOL_H_O6CX5YEN */

#ifndef SYSEX_PROTOCOL_H_O6CX5YEN
#define SYSEX_PROTOCOL_H_O6CX5YEN

#include "etl/optional.h"

#include "./chunk.h"
#include "./codec.h"

// TODO: Currently handles file streaming/writing, as well as sysex decoding.
//       Processing of the byte stream can be offloaded to something external,
//       which keeps this focused on the sysex transport layer, while dealing with
//       the data happens elsewhere. That way, we could change transport later or support
//       multiple transports, like WebSerial etc.

namespace sysex {

// Wraps a file handle to ensure it gets closed through RAII.

template <typename FileOperations> struct Protocol {
  constexpr Protocol(FileOperations &file_ops) : file_ops(file_ops) {
  }

  struct File {
    constexpr File(FileOperations &file_ops, const char *path) : handle(file_ops.open(path)) {
    }

    constexpr size_t write(const etl::array<uint8_t, FileOperations::BlockSize> &bytes,
                           const size_t count) {
      if (handle.has_value()) {
        return handle->write(bytes, count);
      } else {
        return 0;
      }
    }

    constexpr ~File() {
      if (handle.has_value()) {
        handle->close();
        handle.reset();
      }
    }

  private:
    etl::optional<typename FileOperations::Handle> handle;
  };

  enum Tag {
    BeginFileWrite = 0x10,
    FileBytes = 0x11,
    EndFileTransfer = 0x12,
  };

  typedef etl::array<uint16_t, Chunk::Data::SIZE> Values;

  // TODO: Return informative error on failure.
  // Current return value indicates if the message was accepted at all.
  constexpr bool handle_chunk(const Chunk &chunk) {
    Chunk::Data::const_iterator iterator = chunk.cbegin();

    // Make sure it's for us, and we have space for at least a tag
    if (!(chunk.size() >= 4 && (*iterator++) == 0 && (*iterator++) == DatoId &&
          (*iterator++) == DrumId)) {
      return false;
    }

    Values values;
    const auto value_count = codec::decode<Chunk::Data::SIZE>(iterator, chunk.cend(), values);

    if (value_count == 0) {
      return true;
    }

    auto value_iterator = values.cbegin();
    Values::const_iterator values_end = value_iterator + value_count;
    const uint16_t tag = (*value_iterator++);
    if (!handle_no_body(tag) && value_iterator != values_end) {
      handle_packet(tag, value_iterator, values_end);
    } else {
      // TODO: Error?
    }

    return true;
  }

  enum class State {
    Idle,
    FileTransfer,
  };

  // Mainly here for testing. Don't rely on it for external functionality.
  constexpr State __get_state() {
    return state;
  }

private:
  FileOperations &file_ops;
  State state = State::Idle;
  etl::optional<File> opened_file;

  // TODO: Make externally configurable
  static const uint8_t DatoId = 0x7D; // Manufacturer ID for Dato
  static const uint8_t DrumId = 0x65; // Device ID for DRUM
                                      //
  // Handle packets without body
  constexpr bool handle_no_body(const uint16_t tag) {
    switch (state) {
    case State::FileTransfer: {
      if (tag == EndFileTransfer) {
        opened_file.reset();
        state = State::Idle;
        return true;
      }
    } break;
    }

    return false;
  };

  constexpr void handle_packet(const uint16_t tag, Values::const_iterator value_iterator,
                               const Values::const_iterator values_end) {
    switch (state) {
    case State::FileTransfer: {
      if (tag == FileBytes) {
        if (opened_file.has_value()) {
          // We want to write all the 16bit values in one go.
          // If we, for some reason, must support different combinations of sysex size and block
          // size, this code becomes more complicated.
          static_assert(Chunk::Data::SIZE * 2 == FileOperations::BlockSize);

          etl::array<uint8_t, FileOperations::BlockSize> bytes;
          auto byte_iterator = bytes.begin();
          size_t count = 0;
          while (value_iterator != values_end) {
            const uint16_t value = (*value_iterator++);
            const auto tmp_bytes = std::bit_cast<etl::array<uint8_t, 2>>(value);
            (*byte_iterator++) = tmp_bytes[0];
            (*byte_iterator++) = tmp_bytes[1];
            count += 2;
          }

          opened_file->write(bytes, count);
        } else {
          // TODO: Error: Expected file to be open.
        }
      } else {
        opened_file.reset();
        // TODO: Report error
      }
    } break;
    default:
      switch (tag) {
      case BeginFileWrite: {
        // TODO: Error if file is already open, maybe?
        // TODO: Get file path from sysex
        opened_file.emplace(file_ops, "/temp_sample");
        state = State::FileTransfer;
      } break;
      case EndFileTransfer: {
        // Destroyng the file handle should close the file.
        opened_file.reset();
        state = State::Idle;
      } break;
      }
    }
  };
};
} // namespace sysex

#endif /* end of include guard: SYSEX_PROTOCOL_H_O6CX5YEN */

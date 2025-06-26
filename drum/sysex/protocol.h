#ifndef SYSEX_PROTOCOL_H_O6CX5YEN
#define SYSEX_PROTOCOL_H_O6CX5YEN

#include "etl/optional.h"
#include "etl/span.h"
#include "etl/string_view.h"

#include "musin/midi/midi_wrapper.h" // For midi::SystemExclusive

#include "./chunk.h"
#include "./codec.h"

#include <stdio.h>

// TODO: Currently handles file streaming/writing, as well as sysex decoding.
//       Processing of the byte stream can be offloaded to something external,
//       which keeps this focused on the sysex transport layer, while dealing with
//       the data happens elsewhere. That way, we could change transport later or support
//       multiple transports, like WebSerial etc.

namespace sysex {

// Wraps a file handle to ensure it gets closed through RAII.

template <typename FileOperations> struct Protocol {
  static const unsigned MaxFilenameLength = 256;

  constexpr Protocol(FileOperations &file_ops) : file_ops(file_ops) {
  }

  struct File {
    constexpr File(FileOperations &file_ops, const etl::string_view &path)
        : handle(file_ops.open(path)) {
    }

    constexpr bool is_valid() const {
      return handle.has_value();
    }

    constexpr size_t write(const etl::span<const uint8_t> &bytes) {
      if (handle.has_value()) {
        return handle->write(bytes);
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
    Ack = 0x13,
    Nack = 0x14,
    FormatFilesystem = 0x15,
  };

  // TODO: Clean up results to separate successes and errors.
  enum class Result {
    OK,
    FileWritten,
    FileError,
    ShortMessage,
    NotSysex,
    InvalidManufacturer,
    InvalidContent,
    // UnknownTag,
  };

  typedef etl::array<uint16_t, Chunk::Data::SIZE> Values;

  // TODO: Return informative error on failure.
  // Current return value indicates if the message was accepted at all.
  template <typename Sender> constexpr Result handle_chunk(const Chunk &chunk, Sender send_reply) {
    // 5 bytes minimum: 2-byte manufacturer ID (if zero stripped) + 3 bytes for tag.
    if (chunk.size() < 5) {
      printf("SysEx: Error: Short message, size %u\n", (unsigned)chunk.size());
      return Result::ShortMessage;
    }

    Chunk::Data::const_iterator iterator = chunk.cbegin();

    // Check 3-byte manufacturer ID, allowing for stripped leading zero from some MIDI drivers.
    if (chunk.size() >= 3 && (*iterator) == 0 && (*(iterator + 1)) == DatoId &&
        (*(iterator + 2)) == DrumId) {
      iterator += 3;
    } else if (chunk.size() >= 2 && (*iterator) == DatoId && (*(iterator + 1)) == DrumId) {
      iterator += 2;
      printf("SysEx: Warning: Stripped leading zero detected in manufacturer ID\n");
    } else {
      // Not for us
      printf("SysEx: Error: Invalid manufacturer ID\n");
      return Result::InvalidManufacturer;
    }

    Values values{}; // Initialize the values array
    const auto value_count = codec::decode<Chunk::Data::SIZE>(iterator, chunk.cend(), values);

    // Check if we have at least one value for the tag
    if (value_count == 0) {
      printf("SysEx: Error: No values decoded from message\n");
      send_reply(Tag::Nack);
      return Result::InvalidContent;
    }

    auto value_iterator = values.cbegin();
    Values::const_iterator values_end = value_iterator + value_count;
    const uint16_t tag = (*value_iterator++);

    const auto maybe_result = handle_no_body(tag, send_reply);
    if (maybe_result.has_value()) {
      return *maybe_result;
    }

    if (value_iterator != values_end) {
      handle_packet(tag, value_iterator, values_end, send_reply);
    } else {
      send_reply(Tag::Nack);
      return Result::InvalidContent;
    }

    return Result::OK;
  }

  constexpr bool busy() {
    return state != State::Idle;
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

  // Handle packets without body
  template <typename Sender>
  constexpr etl::optional<Result> handle_no_body(const uint16_t tag, Sender send_reply) {
    switch (state) {
    case State::Idle:
      if (tag == Tag::FormatFilesystem) {
        if (file_ops.format()) {
          send_reply(Tag::Ack);
        } else {
          send_reply(Tag::Nack);
        }
        return Result::OK;
      }
      break;
    case State::FileTransfer: {
      if (tag == EndFileTransfer) {
        printf("SysEx: EndFileTransfer received\n");
        opened_file.reset();
        state = State::Idle;
        printf("SysEx: Sending Ack for EndFileTransfer\n");
        send_reply(Tag::Ack);
        return Result::FileWritten;
      }
    } break;
    default:
      break;
    }

    return etl::nullopt;
  };

  template <typename Sender>
  constexpr void handle_packet(const uint16_t tag, Values::const_iterator value_iterator,
                               const Values::const_iterator values_end, Sender send_reply) {
    etl::array<uint8_t, FileOperations::BlockSize> byte_array;
    auto byte_iterator = byte_array.begin();
    size_t byte_count = 0;
    while (value_iterator != values_end) {
      const uint16_t value = (*value_iterator++);
      const auto tmp_bytes = std::bit_cast<etl::array<uint8_t, 2>>(value);
      (*byte_iterator++) = tmp_bytes[0];
      (*byte_iterator++) = tmp_bytes[1];
      byte_count += 2;
    }

    const auto bytes = etl::span{byte_array.cbegin(), byte_count};

    switch (state) {
    case State::FileTransfer: {
      if (tag == FileBytes) {
        if (opened_file.has_value()) {
          // We want to write all the 16bit values in one go.
          // If we, for some reason, must support different combinations of sysex size and block
          // size, this code becomes more complicated.
          static_assert(Chunk::Data::SIZE * 2 == FileOperations::BlockSize);

          opened_file->write(bytes);
          send_reply(Tag::Ack);
        } else {
          // TODO: Error: Expected file to be open.
          printf("SysEx: Error: FileBytes received but no file open\n");
          send_reply(Tag::Nack);
        }
      } else {
        opened_file.reset();
        printf("SysEx: Error: Unexpected tag %u in FileTransfer state\n", tag);
        send_reply(Tag::Nack);
        // TODO: Report error
      }
    } break;
    default:
      switch (tag) {
      case BeginFileWrite: {
        // TODO: Error if file is already open, maybe?

        // TODO: Convert bytes into ASCII string in nicer way.
        //       This is a workaround, since etl::string is not constexpr.
        char path[MaxFilenameLength] = {0}; // Zero-initialize the buffer
        const auto path_length = std::min((size_t)MaxFilenameLength - 1, bytes.size());
        for (unsigned i = 0; i < path_length; ++i) {
          path[i] = bytes[i];
        }
        // The path is already null-terminated due to zero-initialization.

        printf("SysEx: BeginFileWrite received for path: %s\n", path);
        opened_file.emplace(file_ops, path);
        if (opened_file.has_value() && opened_file->is_valid()) {
          state = State::FileTransfer;
          printf("SysEx: Sending Ack for BeginFileWrite\n");
          send_reply(Tag::Ack);
        } else {
          opened_file.reset();
          state = State::Idle;
          printf("SysEx: Error: Failed to open file for writing\n");
          send_reply(Tag::Nack);
        }
      } break;
      case EndFileTransfer: {
        // Destroyng the file handle should close the file.
        opened_file.reset();
        state = State::Idle;
        send_reply(Tag::Ack);
      } break;
      }
    }
  };
};
} // namespace sysex

#endif /* end of include guard: SYSEX_PROTOCOL_H_O6CX5YEN */

#ifndef SYSEX_PROTOCOL_H_O6CX5YEN
#define SYSEX_PROTOCOL_H_O6CX5YEN

#include "drum/config.h"
#include "etl/optional.h"
#include "etl/span.h"
#include "etl/string_view.h"

#include "musin/midi/midi_wrapper.h" // For midi::SystemExclusive

#include "./chunk.h"
#include "./codec.h"
#include "version.h" // For FIRMWARE_MAJOR, etc.

#include <stdio.h>

extern "C" {
#include "pico/unique_id.h" // For pico_get_unique_board_id
}

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
    // Simple Commands (no data payload)
    RequestFirmwareVersion = 0x01,
    RequestSerialNumber = 0x02,
    RebootBootloader = 0x0B,

    // File Transfer Commands
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
    Reboot,
    PrintFirmwareVersion,
    PrintSerialNumber,
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

    // Check for a recognized Manufacturer/Device ID pattern.
    // The chunk passed here has the 0xF0 and 0xF7 bytes stripped.
    if (chunk.size() >= 4 && (*iterator) == drum::config::sysex::MANUFACTURER_ID_0 &&
        (*(iterator + 1)) == drum::config::sysex::MANUFACTURER_ID_1 &&
        (*(iterator + 2)) == drum::config::sysex::MANUFACTURER_ID_2 &&
        (*(iterator + 3)) == drum::config::sysex::DEVICE_ID) {
      iterator += 4; // Official 3-byte ID + 1-byte Device ID
    } else if (chunk.size() >= 3 && (*iterator) == 0 && (*(iterator + 1)) == 0x7D &&
               (*(iterator + 2)) == 0x65) {
      iterator += 3; // Old non-standard 3-byte ID
    } else if (chunk.size() >= 2 && (*iterator) == 0x7D && (*(iterator + 1)) == 0x65) {
      iterator += 2; // Old non-standard 2-byte ID
    } else {
      printf("SysEx: Error: Invalid manufacturer or device ID\n");
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

    // Dispatch based on whether the command has a body or not.
    if (value_iterator != values_end) {
      // Command has a body.
      // printf("SysEx: Command has a body\n");
      return handle_packet(tag, value_iterator, values_end, send_reply);
    } else {
      // Command has no body.
      // printf("SysEx: Command has no body. Tag: %d\n", tag);

      // Handle stateful commands with no body.
      if (state == State::FileTransfer) {
        if (tag == EndFileTransfer) {
          printf("SysEx: EndFileTransfer received\n");
          opened_file.reset();
          state = State::Idle;
          printf("SysEx: Sending Ack for EndFileTransfer\n");
          send_reply(Tag::Ack);
          return Result::FileWritten;
        }
      }

      // Handle stateless commands with no body.
      const auto maybe_result = handle_no_body(tag, send_reply);
      if (maybe_result.has_value()) {
        return *maybe_result;
      }

      // If we reach here, it's an unknown command.
      printf("SysEx: Error: Unknown command with no body. Tag: %u\n", tag);
      if (state == State::FileTransfer) {
        opened_file.reset();
        state = State::Idle;
      }
      send_reply(Tag::Nack);
      return Result::InvalidContent;
    }
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

  // Handle packets without body
  template <typename Sender>
  constexpr etl::optional<Result> handle_no_body(const uint16_t tag, Sender send_reply) {
    // Handle stateless commands that can be executed anytime.
    switch (tag) {
    case Tag::RebootBootloader:
      return Result::Reboot; // Action handled by caller
    case Tag::RequestFirmwareVersion:
      return Result::PrintFirmwareVersion;
    case Tag::RequestSerialNumber:
      return Result::PrintSerialNumber;
    default:
      break; // Not a stateless command, continue to stateful logic.
    }

    // Handle stateful commands that are only valid in Idle state.
    if (tag == Tag::FormatFilesystem) {
      if (state != State::Idle) {
        printf("SysEx: Error: Format command received while not in Idle state.\n");
        send_reply(Tag::Nack);
        return Result::FileError;
      }
      if (file_ops.format()) {
        send_reply(Tag::Ack);
      } else {
        send_reply(Tag::Nack);
      }
      return Result::OK;
    }

    return etl::nullopt;
  };

  template <typename Sender>
  constexpr Result handle_packet(const uint16_t tag, Values::const_iterator value_iterator,
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

    switch (tag) {
    case BeginFileWrite: {
      if (state != State::Idle) {
        printf(
            "SysEx: Error: BeginFileWrite received while another file transfer is in progress.\n");
        send_reply(Tag::Nack);
        return Result::FileError;
      }

      char path[drum::config::MAX_PATH_LENGTH] = {0}; // Zero-initialize the buffer
      const auto path_length = std::min((size_t)drum::config::MAX_PATH_LENGTH - 1, bytes.size());
      for (unsigned i = 0; i < path_length; ++i) {
        path[i] = bytes[i];
      }

      printf("SysEx: BeginFileWrite received for path: %s\n", path);
      opened_file.emplace(file_ops, path);
      if (opened_file.has_value() && opened_file->is_valid()) {
        state = State::FileTransfer;
        printf("SysEx: Sending Ack for BeginFileWrite\n");
        send_reply(Tag::Ack);
        return Result::OK;
      } else {
        opened_file.reset();
        // state is already Idle
        printf("SysEx: Error: Failed to open file for writing\n");
        send_reply(Tag::Nack);
        return Result::FileError;
      }
    } break;

    case FileBytes: {
      if (state != State::FileTransfer) {
        printf("SysEx: Error: FileBytes received while not in a file transfer state.\n");
        send_reply(Tag::Nack);
        return Result::FileError;
      }

      if (opened_file.has_value()) {
        // We want to write all the 16bit values in one go.
        // If we, for some reason, must support different combinations of sysex size and block
        // size, this code becomes more complicated.
        static_assert(Chunk::Data::SIZE * 2 == FileOperations::BlockSize);

        opened_file->write(bytes);
        send_reply(Tag::Ack);
        return Result::OK;
      } else {
        // This case should be theoretically unreachable if state is FileTransfer,
        // but as a safeguard:
        printf("SysEx: Error: FileBytes received but no file open\n");
        send_reply(Tag::Nack);
        state = State::Idle; // Reset state
        return Result::FileError;
      }
    } break;

    default:
      printf("SysEx: Error: Unknown tag %u with body\n", tag);
      if (state == State::FileTransfer) {
        opened_file.reset();
        state = State::Idle;
      }
      send_reply(Tag::Nack);
      return Result::InvalidContent;
    }
  }
};
} // namespace sysex

#endif /* end of include guard: SYSEX_PROTOCOL_H_O6CX5YEN */

#ifndef SYSEX_PROTOCOL_H_O6CX5YEN
#define SYSEX_PROTOCOL_H_O6CX5YEN

#include "drum/config.h"
#include "etl/array.h"
#include "etl/optional.h"
#include "etl/span.h"
#include "etl/string_view.h"

extern "C" {
#include "pico/time.h"
}

#include "musin/hal/logger.h"

#include "./chunk.h"
#include "./codec.h"

namespace sysex {

template <typename FileOperations> struct Protocol {
  static constexpr uint64_t TIMEOUT_US = 5000000; // 5 seconds

  constexpr Protocol(FileOperations &file_ops, musin::Logger &logger)
      : file_ops(file_ops), logger(logger), last_activity_time_{} {
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
  };

  template <typename Sender>
  constexpr Result handle_chunk(const Chunk &chunk, Sender send_reply, absolute_time_t now) {
    if (chunk.size() < 5) {
      logger.error("SysEx: Short message, size", static_cast<uint32_t>(chunk.size()));
      return Result::ShortMessage;
    }

    Chunk::Data::const_iterator iterator = chunk.cbegin();

    if (!check_and_advance_manufacturer_id(iterator, chunk.size())) {
      return Result::InvalidManufacturer;
    }

    const uint16_t tag = (*iterator++);

    if (tag == Tag::FileBytes) {
      return handle_file_bytes_fast(iterator, chunk.cend(), send_reply, now);
    }

    const bool body_was_present = (iterator != chunk.cend());
    etl::array<uint16_t, Chunk::Data::SIZE> values{};
    const auto value_count =
        codec::decode_3_to_16bit(iterator, chunk.cend(), values.begin(), values.end());

    if (value_count == 0 && body_was_present) {
      logger.error("SysEx: Present body could not be decoded.");
      send_reply(Tag::Nack);
      return Result::InvalidContent;
    }

    auto value_iterator = values.cbegin();
    const auto values_end = value_iterator + value_count;

    if (value_iterator != values_end) {
      return handle_packet(tag, value_iterator, values_end, send_reply, now);
    } else {
      if (state == State::FileTransfer) {
        if (tag == EndFileTransfer) {
          logger.info("SysEx: EndFileTransfer received");
          flush_write_buffer();
          opened_file.reset();
          state = State::Idle;
          logger.info("SysEx: Sending Ack for EndFileTransfer");
          send_reply(Tag::Ack);
          return Result::FileWritten;
        }
      }

      const auto maybe_result = handle_no_body(tag, send_reply);
      if (maybe_result.has_value()) {
        return *maybe_result;
      }

      logger.error("SysEx: Unknown command with no body. Tag", tag);
      if (state == State::FileTransfer) {
        opened_file.reset();
        state = State::Idle;
      }
      send_reply(Tag::Nack);
      return Result::InvalidContent;
    }
  }

  constexpr bool check_timeout(absolute_time_t now) {
    if (state == State::FileTransfer) {
      if (absolute_time_diff_us(last_activity_time_, now) > TIMEOUT_US) {
        logger.warn("SysEx: File transfer timed out.");
        flush_write_buffer();
        opened_file.reset();
        state = State::Idle;
        return true;
      }
    }
    return false;
  }

  constexpr bool busy() {
    return state != State::Idle;
  }

  enum class State {
    Idle,
    FileTransfer,
  };

  constexpr State get_state() {
    return state;
  }

private:
  FileOperations &file_ops;
  musin::Logger &logger;
  State state = State::Idle;
  etl::optional<File> opened_file;
  absolute_time_t last_activity_time_;

  etl::array<uint8_t, FileOperations::BlockSize> write_buffer;
  size_t write_buffer_pos = 0;

  enum class SanitizeResult {
    Success,
    PathTooLong,
    InvalidCharacter
  };

  template <typename Sender>
  constexpr etl::optional<Result> handle_no_body(const uint16_t tag, Sender send_reply) {
    switch (tag) {
    case Tag::RebootBootloader:
      return Result::Reboot;
    case Tag::RequestFirmwareVersion:
      return Result::PrintFirmwareVersion;
    case Tag::RequestSerialNumber:
      return Result::PrintSerialNumber;
    default:
      break;
    }

    if (tag == Tag::FormatFilesystem) {
      if (state != State::Idle) {
        logger.error("SysEx: Format command received while not in Idle state.");
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

  template <typename Sender, typename ValueIt>
  constexpr Result handle_packet(const uint16_t tag, ValueIt value_iterator,
                                 const ValueIt values_end, Sender send_reply,
                                 absolute_time_t now) {
    etl::array<uint8_t, FileOperations::BlockSize> byte_array;
    auto byte_iterator = byte_array.begin();
    size_t byte_count = 0;
    while (value_iterator != values_end) {
      const uint16_t value = (*value_iterator++);
      const auto value_bytes = std::bit_cast<etl::array<uint8_t, 2>>(value);
      (*byte_iterator++) = value_bytes[0];
      (*byte_iterator++) = value_bytes[1];
      byte_count += 2;
    }

    const auto bytes = etl::span{byte_array.cbegin(), byte_count};

    switch (tag) {
    case BeginFileWrite:
      return handle_begin_file_write(bytes, send_reply, now);
    default:
      logger.error("SysEx: Unknown tag with body", tag);
      if (state == State::FileTransfer) {
        opened_file.reset();
        state = State::Idle;
      }
      send_reply(Tag::Nack);
      return Result::InvalidContent;
    }
  }

  template <typename Sender>
  constexpr Result handle_begin_file_write(const etl::span<const uint8_t> &bytes, Sender send_reply,
                                           absolute_time_t now) {
    if (state != State::Idle) {
      logger.warn("SysEx: BeginFileWrite received while another file transfer is in progress. "
                  "Canceling previous transfer.");
      flush_write_buffer();
      opened_file.reset();
    }

    char path[drum::config::MAX_PATH_LENGTH];
    const auto sanitize_result = sanitize_path(bytes, path);

    if (sanitize_result != SanitizeResult::Success) {
      send_reply(Tag::Nack);
      return Result::FileError;
    }

    logger.info("SysEx: BeginFileWrite received for path:");
    logger.info(path);
    opened_file.emplace(file_ops, path);
    if (opened_file.has_value() && opened_file->is_valid()) {
      state = State::FileTransfer;
      write_buffer_pos = 0;
      last_activity_time_ = now;
      logger.info("SysEx: Sending Ack for BeginFileWrite");
      send_reply(Tag::Ack);
      return Result::OK;
    } else {
      opened_file.reset();
      state = State::Idle;
      logger.error("SysEx: Failed to open file for writing");
      send_reply(Tag::Nack);
      return Result::FileError;
    }
  }

  template <typename Sender, typename InputIt>
  constexpr Result handle_file_bytes_fast(InputIt start, InputIt end, Sender send_reply,
                                          absolute_time_t now) {
    if (state != State::FileTransfer) {
      logger.error("SysEx: FileBytes received while not in a file transfer state.");
      send_reply(Tag::Nack);
      return Result::FileError;
    }

    if (opened_file.has_value()) {
      const size_t bytes_decoded = codec::decode_8_to_7(
          start, end, write_buffer.begin() + write_buffer_pos, write_buffer.end());
      write_buffer_pos += bytes_decoded;

      if (write_buffer_pos >= write_buffer.size()) {
        flush_write_buffer();
      }
      last_activity_time_ = now;
      send_reply(Tag::Ack);
      return Result::OK;
    } else {
      logger.error("SysEx: FileBytes received but no file open");
      send_reply(Tag::Nack);
      state = State::Idle;
      return Result::FileError;
    }
  }

  constexpr void flush_write_buffer() {
    if (opened_file.has_value() && write_buffer_pos > 0) {
      opened_file->write(etl::span{write_buffer.cbegin(), write_buffer_pos});
      write_buffer_pos = 0;
    }
  }

  constexpr bool check_and_advance_manufacturer_id(Chunk::Data::const_iterator &iterator,
                                                   const size_t chunk_size) const {
    if (chunk_size >= 4 && (*iterator) == drum::config::sysex::MANUFACTURER_ID_0 &&
        (*(iterator + 1)) == drum::config::sysex::MANUFACTURER_ID_1 &&
        (*(iterator + 2)) == drum::config::sysex::MANUFACTURER_ID_2 &&
        (*(iterator + 3)) == drum::config::sysex::DEVICE_ID) {
      iterator += 4;
      return true;
    }
    logger.error("SysEx: Invalid manufacturer or device ID");
    return false;
  }

  static constexpr SanitizeResult sanitize_path(const etl::span<const uint8_t> &raw_path,
                                                char (&out_path)[drum::config::MAX_PATH_LENGTH]) {
    for (size_t i = 0; i < drum::config::MAX_PATH_LENGTH; ++i) {
      out_path[i] = 0;
    }

    size_t out_pos = 0;
    out_path[out_pos++] = '/';

    size_t in_pos = 0;
    if (!raw_path.empty() && raw_path[0] == '/') {
      in_pos = 1;
    }

    for (; in_pos < raw_path.size() && raw_path[in_pos] != '\0'; ++in_pos) {
      if (out_pos >= drum::config::MAX_PATH_LENGTH - 1) {
        return SanitizeResult::PathTooLong;
      }

      const char character = static_cast<char>(raw_path[in_pos]);

      if (character == '/' || character < ' ' || character > '~') {
        return SanitizeResult::InvalidCharacter;
      }
      out_path[out_pos++] = character;
    }
    return SanitizeResult::Success;
  }
};
} // namespace sysex

#endif /* end of include guard: SYSEX_PROTOCOL_H_O6CX5YEN */

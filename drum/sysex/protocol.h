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

#include "musin/midi/midi_wrapper.h"

#include "./chunk.h"
#include "./codec.h"
#include "./uf2_validator.h"

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
    RequestStorageInfo = 0x03,
    StorageInfoResponse = 0x04,
    RebootBootloader = 0x0B,

    // File Transfer Commands
    BeginFileWrite = 0x10,
    FileBytes = 0x11,
    EndFileTransfer = 0x12,
    Ack = 0x13,
    Nack = 0x14,
    FormatFilesystem = 0x15,

    // Firmware Transfer Commands
    BeginFirmwareWrite = 0x20,
  };

  enum class Result {
    OK,
    FileWritten,
    Reboot,
    PrintFirmwareVersion,
    PrintSerialNumber,
    PrintStorageInfo,
    FileError,
    ShortMessage,
    NotSysex,
    InvalidManufacturer,
    InvalidContent,
    FirmwareVerified,
    FirmwareVerificationFailed,
  };

  template <typename Sender>
  constexpr Result handle_chunk(const Chunk &chunk, Sender send_reply,
                                absolute_time_t now) {
    if (chunk.size() < 5) {
      logger.error("SysEx: Short message, size",
                   static_cast<uint32_t>(chunk.size()));
      return Result::ShortMessage;
    }

    Chunk::const_iterator iterator = chunk.cbegin();

    if (!check_and_advance_manufacturer_id(iterator, chunk.cend())) {
      return Result::InvalidManufacturer;
    }

    // Fast path for FileBytes, which is the most common command during a
    // transfer. This avoids the overhead of the 3-to-16bit decoding.
    if (is_file_bytes_command(chunk)) {
      return handle_file_bytes_fast(iterator + 1, chunk.cend(), send_reply,
                                    now);
    }

    const uint16_t tag = (*iterator++);

    const bool body_was_present = (iterator != chunk.cend());
    etl::array<uint16_t, MIDI::SysExMaxSize> values{};
    const auto value_count = codec::decode_3_to_16bit(
        iterator, chunk.cend(), values.begin(), values.end());

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

      if (state == State::FirmwareTransfer) {
        if (tag == EndFileTransfer) {
          logger.info("SysEx: EndFileTransfer received for firmware");
          flush_write_buffer();

          if (firmware_state.has_value()) {
            const bool all_blocks =
                firmware_state->validator.all_blocks_received();
            const uint32_t expected =
                firmware_state->validator.get_expected_blocks();
            const uint32_t received =
                firmware_state->validator.get_received_count();

            logger.info("SysEx: UF2 blocks received:", received);
            logger.info("SysEx: UF2 blocks expected:", expected);

            if (!all_blocks) {
              logger.error(
                  "SysEx: Firmware transfer incomplete, missing blocks");
              opened_file.reset();
              firmware_state.reset();
              state = State::Idle;
              send_reply(Tag::Nack);
              return Result::FirmwareVerificationFailed;
            }

            logger.info("SysEx: All UF2 blocks received successfully");
            opened_file.reset();
            firmware_state.reset();
            state = State::Idle;
            logger.info("SysEx: Sending Ack for firmware EndFileTransfer");
            send_reply(Tag::Ack);
            return Result::FirmwareVerified;
          } else {
            logger.error(
                "SysEx: Firmware state missing during EndFileTransfer");
            opened_file.reset();
            firmware_state.reset();
            state = State::Idle;
            send_reply(Tag::Nack);
            return Result::FirmwareVerificationFailed;
          }
        }
      }

      const auto maybe_result = handle_no_body(tag, send_reply);
      if (maybe_result.has_value()) {
        return *maybe_result;
      }

      logger.error("SysEx: Unknown command with no body. Tag",
                   static_cast<uint32_t>(tag));
      if (state == State::FileTransfer || state == State::FirmwareTransfer) {
        opened_file.reset();
        firmware_state.reset();
        state = State::Idle;
      }
      send_reply(Tag::Nack);
      return Result::InvalidContent;
    }
  }

  constexpr bool check_timeout(absolute_time_t now) {
    if (state == State::FileTransfer || state == State::FirmwareTransfer) {
      if (absolute_time_diff_us(last_activity_time_, now) >
          static_cast<int64_t>(TIMEOUT_US)) {
        logger.warn("SysEx: Transfer timed out.");
        flush_write_buffer();
        opened_file.reset();
        firmware_state.reset();
        state = State::Idle;
        return true;
      }
    }
    return false;
  }

  constexpr bool busy() const {
    return state != State::Idle;
  }

  enum class State {
    Idle,
    FileTransfer,
    FirmwareTransfer,
  };

  constexpr State get_state() const {
    return state;
  }

private:
  struct FirmwareTransferState {
    size_t expected_size;
    uint32_t expected_crc32;
    size_t bytes_written;
    UF2BlockValidator validator;

    constexpr FirmwareTransferState(size_t size, uint32_t crc32)
        : expected_size(size), expected_crc32(crc32), bytes_written(0) {
    }
  };

  FileOperations &file_ops;
  musin::Logger &logger;
  State state = State::Idle;
  etl::optional<File> opened_file;
  etl::optional<FirmwareTransferState> firmware_state;
  absolute_time_t last_activity_time_;

  etl::array<uint8_t, FileOperations::BlockSize> write_buffer;
  size_t write_buffer_pos = 0;

  enum class SanitizeResult {
    Success,
    PathTooLong,
    InvalidCharacter
  };

  static constexpr uint8_t get_tag_from_chunk(const Chunk &chunk) {
    return chunk[4];
  }

  constexpr bool is_file_bytes_command(const Chunk &chunk) const {
    return get_tag_from_chunk(chunk) == Tag::FileBytes;
  }

  template <typename Sender>
  constexpr etl::optional<Result> handle_no_body(const uint16_t tag,
                                                 Sender send_reply) {
    switch (tag) {
    case Tag::RebootBootloader:
      return Result::Reboot;
    case Tag::RequestFirmwareVersion:
      return Result::PrintFirmwareVersion;
    case Tag::RequestSerialNumber:
      return Result::PrintSerialNumber;
    case Tag::RequestStorageInfo:
      return Result::PrintStorageInfo;
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
    case BeginFirmwareWrite:
      return handle_begin_firmware_write(bytes, send_reply, now);
    default:
      logger.error("SysEx: Unknown tag with body", static_cast<uint32_t>(tag));
      if (state == State::FileTransfer || state == State::FirmwareTransfer) {
        opened_file.reset();
        firmware_state.reset();
        state = State::Idle;
      }
      send_reply(Tag::Nack);
      return Result::InvalidContent;
    }
  }

  template <typename Sender>
  constexpr Result
  handle_begin_file_write(const etl::span<const uint8_t> &bytes,
                          Sender send_reply, absolute_time_t now) {
    if (state != State::Idle) {
      logger.warn("SysEx: BeginFileWrite received while another file transfer "
                  "is in progress. "
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

  template <typename Sender>
  constexpr Result
  handle_begin_firmware_write(const etl::span<const uint8_t> &bytes,
                              Sender send_reply, absolute_time_t now) {
    if (state != State::Idle) {
      logger.warn("SysEx: BeginFirmwareWrite received while another transfer "
                  "is in progress. Canceling previous transfer.");
      flush_write_buffer();
      opened_file.reset();
      firmware_state.reset();
    }

    if (bytes.size() < 9) {
      logger.error("SysEx: BeginFirmwareWrite payload too short");
      send_reply(Tag::Nack);
      return Result::FileError;
    }

    size_t path_end = 0;
    while (path_end < bytes.size() && bytes[path_end] != '\0') {
      path_end++;
    }

    if (path_end == 0 || path_end + 8 > bytes.size()) {
      logger.error("SysEx: BeginFirmwareWrite invalid payload format");
      send_reply(Tag::Nack);
      return Result::FileError;
    }

    const auto path_bytes = etl::span{bytes.begin(), path_end};

    char path[drum::config::MAX_PATH_LENGTH];
    const auto sanitize_result = sanitize_path(path_bytes, path);

    if (sanitize_result != SanitizeResult::Success) {
      logger.error("SysEx: BeginFirmwareWrite path sanitization failed");
      send_reply(Tag::Nack);
      return Result::FileError;
    }

    constexpr char firmware_prefix[] = "/firmware/";
    bool has_firmware_prefix = true;
    for (size_t i = 0; i < sizeof(firmware_prefix) - 1; ++i) {
      if (path[i] != firmware_prefix[i]) {
        has_firmware_prefix = false;
        break;
      }
    }

    if (!has_firmware_prefix) {
      logger.error("SysEx: BeginFirmwareWrite path must start with /firmware/");
      send_reply(Tag::Nack);
      return Result::FileError;
    }

    const size_t size_offset = path_end + 1;
    const uint32_t expected_size =
        static_cast<uint32_t>(bytes[size_offset]) |
        (static_cast<uint32_t>(bytes[size_offset + 1]) << 8) |
        (static_cast<uint32_t>(bytes[size_offset + 2]) << 16) |
        (static_cast<uint32_t>(bytes[size_offset + 3]) << 24);

    const uint32_t expected_crc32 =
        static_cast<uint32_t>(bytes[size_offset + 4]) |
        (static_cast<uint32_t>(bytes[size_offset + 5]) << 8) |
        (static_cast<uint32_t>(bytes[size_offset + 6]) << 16) |
        (static_cast<uint32_t>(bytes[size_offset + 7]) << 24);

    logger.info("SysEx: BeginFirmwareWrite received for path:");
    logger.info(path);
    logger.info("SysEx: Expected size:", expected_size);
    logger.info("SysEx: Expected CRC32:", expected_crc32);

    opened_file.emplace(file_ops, path);
    if (opened_file.has_value() && opened_file->is_valid()) {
      state = State::FirmwareTransfer;
      firmware_state.emplace(expected_size, expected_crc32);
      write_buffer_pos = 0;
      last_activity_time_ = now;
      logger.info("SysEx: Sending Ack for BeginFirmwareWrite");
      send_reply(Tag::Ack);
      return Result::OK;
    } else {
      opened_file.reset();
      firmware_state.reset();
      state = State::Idle;
      logger.error("SysEx: Failed to open file for firmware writing");
      send_reply(Tag::Nack);
      return Result::FileError;
    }
  }

  template <typename Sender, typename InputIt>
  constexpr Result handle_file_bytes_fast(InputIt start, InputIt end,
                                          Sender send_reply,
                                          absolute_time_t now) {
    if (state != State::FileTransfer && state != State::FirmwareTransfer) {
      logger.error(
          "SysEx: FileBytes received while not in a file transfer state.");
      send_reply(Tag::Nack);
      return Result::FileError;
    }

    if (opened_file.has_value()) {
      while (start != end) {
        const auto result = codec::decode_8_to_7(
            start, end, write_buffer.begin() + write_buffer_pos,
            write_buffer.end());

        const size_t bytes_read = result.first;
        const size_t bytes_decoded = result.second;

        write_buffer_pos += bytes_decoded;
        start += bytes_read;

        if (state == State::FirmwareTransfer) {
          constexpr size_t UF2_BLOCK_SIZE = 512;
          while (write_buffer_pos >= UF2_BLOCK_SIZE) {
            const auto *block_ptr =
                reinterpret_cast<const uf2_block *>(write_buffer.data());
            if (firmware_state.has_value()) {
              const auto validation_result =
                  firmware_state->validator.validate_block(*block_ptr);
              if (validation_result !=
                  UF2BlockValidator::ValidationResult::Success) {
                logger.error("SysEx: UF2 block validation failed:",
                             static_cast<uint32_t>(validation_result));
                opened_file.reset();
                firmware_state.reset();
                state = State::Idle;
                send_reply(Tag::Nack);
                return Result::FirmwareVerificationFailed;
              }
            }

            if (!flush_write_buffer()) {
              logger.error("SysEx: Failed to write buffer, aborting transfer.");
              opened_file.reset();
              firmware_state.reset();
              state = State::Idle;
              send_reply(Tag::Nack);
              return Result::FileError;
            }
          }
        }

        if (write_buffer_pos >= write_buffer.size()) {
          if (!flush_write_buffer()) {
            logger.error("SysEx: Failed to write buffer, aborting transfer.");
            opened_file.reset();
            if (state == State::FirmwareTransfer) {
              firmware_state.reset();
            }
            state = State::Idle;
            send_reply(Tag::Nack);
            return Result::FileError;
          }
        }
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

  constexpr bool flush_write_buffer() {
    if (opened_file.has_value() && write_buffer_pos > 0) {
      const size_t written = opened_file->write(
          etl::span{write_buffer.cbegin(), write_buffer_pos});
      if (written != write_buffer_pos) {
        logger.error("SysEx: Failed to write all bytes to file.");
        return false;
      }
      write_buffer_pos = 0;
    }
    return true;
  }

  constexpr bool
  check_and_advance_manufacturer_id(Chunk::const_iterator &iterator,
                                    Chunk::const_iterator end) const {
    if (static_cast<size_t>(etl::distance(iterator, end)) >= 4 &&
        (*iterator) == drum::config::sysex::MANUFACTURER_ID_0 &&
        (*(iterator + 1)) == drum::config::sysex::MANUFACTURER_ID_1 &&
        (*(iterator + 2)) == drum::config::sysex::MANUFACTURER_ID_2 &&
        (*(iterator + 3)) == drum::config::sysex::DEVICE_ID) {
      iterator += 4;
      return true;
    }
    logger.error("SysEx: Invalid manufacturer or device ID");
    return false;
  }

  static constexpr SanitizeResult
  sanitize_path(const etl::span<const uint8_t> &raw_path,
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

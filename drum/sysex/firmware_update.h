#ifndef SYSEX_FIRMWARE_UPDATE_H_KQ8WP4TR
#define SYSEX_FIRMWARE_UPDATE_H_KQ8WP4TR

#include "drum/config.h"
#include "etl/array.h"
#include "etl/span.h"

extern "C" {
#include "pico/time.h"
}

#include "musin/hal/logger.h"

#include "./codec.h"
#include "musin/midi/sysex_chunk.h"

namespace sysex {

// State machine for firmware transfer to the inactive A/B partition.
//
// The Writer dependency receives the raw UF2 byte stream and owns parsing,
// flash programming and SHA-256 verification:
//   bool begin(uint32_t total_size, etl::span<const uint8_t> sha256);
//   bool write(etl::span<const uint8_t> bytes);
//   bool finalize();
//   void abort();
template <typename Writer> struct FirmwareUpdate {
  static constexpr uint64_t TIMEOUT_US = 5000000; // 5 seconds
  static constexpr size_t SHA256_SIZE = 32;
  // total size (4 bytes) + SHA-256 + version major/minor/patch
  static constexpr size_t BEGIN_PAYLOAD_SIZE = 4 + SHA256_SIZE + 3;

  enum Tag {
    BeginFirmwareUpdate = 0x20,
    FirmwareBytes = 0x21,
    EndFirmwareUpdate = 0x22,
    AbortFirmwareUpdate = 0x23,
    Ack = 0x13,
    Nack = 0x14,
  };

  enum class Result {
    OK,
    UpdateReady, // EndFirmwareUpdate verified; caller may reboot into trial
    NotFirmwareUpdate,
    InvalidContent,
    WriteError,
    Aborted,
  };

  enum class State {
    Idle,
    Receiving,
  };

  constexpr FirmwareUpdate(Writer &writer, musin::Logger &logger)
      : writer_(writer), logger_(logger), last_activity_time_{} {
  }

  // Returns true for chunks this state machine should handle (correct
  // manufacturer/device header and a firmware-update tag).
  static constexpr bool claims(const Chunk &chunk) {
    if (chunk.size() < 5) {
      return false;
    }
    if (chunk[0] != drum::config::sysex::MANUFACTURER_ID_0 ||
        chunk[1] != drum::config::sysex::MANUFACTURER_ID_1 ||
        chunk[2] != drum::config::sysex::MANUFACTURER_ID_2 ||
        chunk[3] != drum::config::sysex::DEVICE_ID) {
      return false;
    }
    const uint8_t tag = chunk[4];
    return tag >= Tag::BeginFirmwareUpdate && tag <= Tag::AbortFirmwareUpdate;
  }

  template <typename Sender>
  constexpr Result handle_chunk(const Chunk &chunk, Sender &&send_reply,
                                absolute_time_t now) {
    if (!claims(chunk)) {
      return Result::NotFirmwareUpdate;
    }

    const uint8_t tag = chunk[4];
    const auto body =
        etl::span<const uint8_t>{chunk.cbegin() + 5, chunk.cend()};

    switch (tag) {
    case Tag::BeginFirmwareUpdate:
      return handle_begin(body, send_reply, now);
    case Tag::FirmwareBytes:
      return handle_bytes(body, send_reply, now);
    case Tag::EndFirmwareUpdate:
      return handle_end(send_reply);
    case Tag::AbortFirmwareUpdate:
      return handle_abort(send_reply);
    default:
      return Result::NotFirmwareUpdate;
    }
  }

  constexpr bool check_timeout(absolute_time_t now) {
    if (state_ == State::Receiving &&
        absolute_time_diff_us(last_activity_time_, now) >
            static_cast<int64_t>(TIMEOUT_US)) {
      logger_.warn("SysEx: Firmware update timed out.");
      writer_.abort();
      state_ = State::Idle;
      return true;
    }
    return false;
  }

  constexpr bool busy() const {
    return state_ != State::Idle;
  }

  constexpr State get_state() const {
    return state_;
  }

  // Bytes accepted so far; with the total from BeginFirmwareUpdate this
  // drives progress display.
  constexpr uint32_t bytes_received() const {
    return bytes_received_;
  }

  constexpr uint32_t total_size() const {
    return total_size_;
  }

private:
  template <typename Sender>
  constexpr Result handle_begin(etl::span<const uint8_t> encoded_body,
                                Sender &send_reply, absolute_time_t now) {
    if (state_ != State::Idle) {
      logger_.warn("SysEx: BeginFirmwareUpdate during active update; "
                   "restarting.");
      writer_.abort();
      state_ = State::Idle;
    }

    // The Begin payload uses the same 3-byte-to-16-bit encoding as
    // BeginFileWrite; each decoded value carries two payload bytes.
    etl::array<uint16_t, BEGIN_PAYLOAD_SIZE / 2 + 1> values{};
    const auto value_count = codec::decode_3_to_16bit(
        encoded_body.begin(), encoded_body.end(), values.begin(), values.end());

    etl::array<uint8_t, sizeof(values)> bytes{};
    size_t byte_count = 0;
    for (size_t i = 0; i < value_count; ++i) {
      bytes[byte_count++] = static_cast<uint8_t>(values[i] & 0xFF);
      bytes[byte_count++] = static_cast<uint8_t>(values[i] >> 8);
    }

    if (byte_count < BEGIN_PAYLOAD_SIZE) {
      logger_.error("SysEx: BeginFirmwareUpdate payload too short",
                    static_cast<uint32_t>(byte_count));
      send_reply(Tag::Nack);
      return Result::InvalidContent;
    }

    const uint32_t total_size = static_cast<uint32_t>(bytes[0]) |
                                (static_cast<uint32_t>(bytes[1]) << 8) |
                                (static_cast<uint32_t>(bytes[2]) << 16) |
                                (static_cast<uint32_t>(bytes[3]) << 24);
    const auto sha256 = etl::span<const uint8_t>{bytes.data() + 4, SHA256_SIZE};

    if (total_size == 0 || !writer_.begin(total_size, sha256)) {
      logger_.error("SysEx: BeginFirmwareUpdate rejected by writer");
      send_reply(Tag::Nack);
      return Result::WriteError;
    }

    total_size_ = total_size;
    bytes_received_ = 0;
    state_ = State::Receiving;
    last_activity_time_ = now;
    logger_.info("SysEx: Firmware update started, size", total_size);
    send_reply(Tag::Ack);
    return Result::OK;
  }

  template <typename Sender, typename InputIt>
  constexpr Result decode_and_write(InputIt start, InputIt end,
                                    Sender &send_reply) {
    while (start != end) {
      const auto result = codec::decode_8_to_7(
          start, end, decode_buffer_.begin(), decode_buffer_.end());
      const size_t bytes_read = result.first;
      const size_t bytes_decoded = result.second;
      if (bytes_decoded == 0) {
        break;
      }
      start += bytes_read;

      // The 8-to-7 codec decodes whole 7-byte groups, so the final group of
      // the stream may carry up to 6 padding bytes beyond the announced size.
      const size_t remaining = total_size_ - bytes_received_;
      const size_t to_write = etl::min(bytes_decoded, remaining);
      const size_t excess = bytes_decoded - to_write;
      if (excess >= 7) {
        logger_.error("SysEx: FirmwareBytes exceed announced size");
        writer_.abort();
        state_ = State::Idle;
        send_reply(Tag::Nack);
        return Result::WriteError;
      }

      if (to_write > 0 && !writer_.write(etl::span<const uint8_t>{
                              decode_buffer_.data(), to_write})) {
        logger_.error("SysEx: Firmware write failed, aborting update.");
        writer_.abort();
        state_ = State::Idle;
        send_reply(Tag::Nack);
        return Result::WriteError;
      }
      bytes_received_ += to_write;
    }
    return Result::OK;
  }

  template <typename Sender>
  constexpr Result handle_bytes(etl::span<const uint8_t> body,
                                Sender &send_reply, absolute_time_t now) {
    if (state_ != State::Receiving) {
      logger_.error("SysEx: FirmwareBytes received while not updating.");
      send_reply(Tag::Nack);
      return Result::InvalidContent;
    }

    const Result result =
        decode_and_write(body.begin(), body.end(), send_reply);
    if (result != Result::OK) {
      return result;
    }

    last_activity_time_ = now;
    // Ack only after the data has been written through to flash so the host
    // self-throttles to the device's flash speed.
    send_reply(Tag::Ack);
    return Result::OK;
  }

  template <typename Sender> constexpr Result handle_end(Sender &send_reply) {
    if (state_ != State::Receiving) {
      logger_.error("SysEx: EndFirmwareUpdate received while not updating.");
      send_reply(Tag::Nack);
      return Result::InvalidContent;
    }

    state_ = State::Idle;
    if (bytes_received_ != total_size_ || !writer_.finalize()) {
      logger_.error("SysEx: Firmware verification failed.");
      writer_.abort();
      send_reply(Tag::Nack);
      return Result::WriteError;
    }

    logger_.info("SysEx: Firmware update verified.");
    send_reply(Tag::Ack);
    return Result::UpdateReady;
  }

  template <typename Sender> constexpr Result handle_abort(Sender &send_reply) {
    if (state_ == State::Receiving) {
      writer_.abort();
      state_ = State::Idle;
    }
    logger_.info("SysEx: Firmware update aborted by host.");
    send_reply(Tag::Ack);
    return Result::Aborted;
  }

  Writer &writer_;
  musin::Logger &logger_;
  State state_ = State::Idle;
  absolute_time_t last_activity_time_;
  uint32_t total_size_ = 0;
  uint32_t bytes_received_ = 0;
  // Large enough for a full 2048-byte SysEx message decoded 8-to-7.
  etl::array<uint8_t, 1792> decode_buffer_{};
};

} // namespace sysex

#endif /* end of include guard: SYSEX_FIRMWARE_UPDATE_H_KQ8WP4TR */

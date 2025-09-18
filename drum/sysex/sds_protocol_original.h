#ifndef SDS_PROTOCOL_H_SDS_AUDIO
#define SDS_PROTOCOL_H_SDS_AUDIO

/**
 * @file sds_protocol.h
 * @brief MIDI Sample Dump Standard (SDS) protocol implementation
 *
 * This implements a minimal subset of the SDS specification for receiving
 * 16-bit PCM audio samples without the padding corruption issues of the
 * custom SysEx protocol.
 *
 * Supported features:
 * - Dump Header parsing with basic sample metadata
 * - Data Packet processing with 16-bit sample unpacking
 * - ACK/NAK response generation
 * - Checksum validation
 * - Integration with existing file operations
 */

#include "drum/firmware/update_interfaces.h"
#include "drum/sysex/codec.h"
#include "etl/array.h"
#include "etl/optional.h"
#include "etl/span.h"
#include "etl/string_view.h"

#include <cstdint>

extern "C" {
#include "pico/time.h"
}

#include "musin/hal/logger.h"

namespace sds {

// SDS Message Types
enum MessageType : uint8_t {
  DUMP_HEADER = 0x01,
  DATA_PACKET = 0x02,
  DUMP_REQUEST = 0x03,
  ACK = 0x7F,
  NAK = 0x7E,
  CANCEL = 0x7D,
  WAIT = 0x7C
};

// SDS Protocol State
enum class State {
  Idle,
  ReceivingHeader,
  ReceivingData
};

enum class TransferType {
  None,
  Sample,
  Firmware
};

// SDS Protocol Result
enum class Result {
  OK,
  SampleComplete,
  FirmwareComplete,
  Cancelled,
  InvalidMessage,
  ChecksumError,
  FileError,
  FlashError,
  PartitionError,
  StateError
};

// Sample metadata from Dump Header
struct SampleInfo {
  uint16_t sample_number;
  uint8_t bit_depth;
  uint32_t sample_period_ns;
  uint32_t length_words;
  uint32_t loop_start;
  uint32_t loop_end;
  uint8_t loop_type;

  constexpr uint32_t get_sample_rate() const {
    return sample_period_ns > 0 ? (1000000000U / sample_period_ns) : 44100;
  }

  constexpr uint32_t get_byte_length() const {
    return length_words * 2; // Words are 2 bytes each
  }
};

template <typename FileOperations> class Protocol {
public:
  constexpr Protocol(FileOperations &file_ops, musin::Logger &logger)
      : file_ops_(file_ops), logger_(logger), state_(State::Idle),
        transfer_type_(TransferType::None), partition_manager_(nullptr),
        flash_writer_(nullptr), expected_packet_num_(0), bytes_received_(0),
        firmware_checksum_accumulator_(0) {
  }

  // Process incoming SDS message
  template <typename Sender>
  constexpr Result process_message(const etl::span<const uint8_t> &message,
                                   Sender send_reply, absolute_time_t now) {
    if (message.size() < 3) {
      return Result::InvalidMessage;
    }

    const uint8_t message_type = message[0];

    switch (message_type) {
    case DUMP_HEADER:
      return handle_dump_header(message, send_reply, now);
    case DATA_PACKET:
      return handle_data_packet(message, send_reply, now);
    case CANCEL:
      return handle_cancel_message();
    default:
      logger_.warn("SDS: Unknown message type:",
                   static_cast<uint32_t>(message_type));
      send_reply(NAK, 0);
      return Result::InvalidMessage;
    }
  }

  constexpr State get_state() const {
    return state_;
  }
  constexpr bool is_busy() const {
    return state_ != State::Idle;
  }

  void
  attach_firmware_targets(drum::firmware::FirmwarePartitionManager &manager,
                          drum::firmware::PartitionFlashWriter &writer) {
    partition_manager_ = &manager;
    flash_writer_ = &writer;
  }

  void detach_firmware_targets() {
    partition_manager_ = nullptr;
    flash_writer_ = nullptr;
  }

private:
  FileOperations &file_ops_;
  musin::Logger &logger_;
  State state_;
  TransferType transfer_type_;
  drum::firmware::FirmwarePartitionManager *partition_manager_;
  drum::firmware::PartitionFlashWriter *flash_writer_;
  uint8_t expected_packet_num_;
  uint32_t bytes_received_;
  SampleInfo current_sample_;

  // File handle wrapper (same pattern as existing protocol)
  struct File {
    constexpr File(FileOperations &file_ops, const etl::string_view &path)
        : handle_(file_ops.open(path)) {
    }

    constexpr bool is_valid() const {
      return handle_.has_value();
    }

    constexpr size_t write(const etl::span<const uint8_t> &bytes) {
      return handle_.has_value() ? handle_->write(bytes) : 0;
    }

    constexpr ~File() {
      if (handle_.has_value()) {
        handle_->close();
        handle_.reset();
      }
    }

  private:
    etl::optional<typename FileOperations::Handle> handle_;
  };

  etl::optional<File> opened_file_;
  static constexpr uint16_t FIRMWARE_HEADER_TOKEN = 0x3FFF;

  static constexpr uint32_t combine_checksum_fields(uint32_t high21,
                                                    uint32_t low21) {
    const uint32_t high_bits = high21 & 0x7FFu; // lower 11 bits
    return (high_bits << 21) | (low21 & 0x1FFFFFu);
  }

  static constexpr etl::optional<drum::firmware::FirmwareImageMetadata>
  parse_firmware_header(const etl::span<const uint8_t> &message) {
    if (message.size() < 17) {
      return etl::nullopt;
    }

    if (parse_14bit(message[1], message[2]) != FIRMWARE_HEADER_TOKEN) {
      return etl::nullopt;
    }

    drum::firmware::FirmwareImageMetadata metadata{};
    metadata.format_version = message[3] & 0x7F;
    metadata.declared_size = parse_21bit(message[4], message[5], message[6]);

    const uint32_t checksum_high =
        parse_21bit(message[7], message[8], message[9]);
    const uint32_t checksum_low =
        parse_21bit(message[10], message[11], message[12]);
    metadata.checksum = combine_checksum_fields(checksum_high, checksum_low);

    metadata.version_tag = parse_21bit(message[13], message[14], message[15]);
    metadata.partition_hint = message[16] & 0x7F;

    if (metadata.declared_size == 0U) {
      return etl::nullopt;
    }

    return metadata;
  }

  void reset_firmware_state() {
    firmware_transfer_ = FirmwareTransfer{};
    firmware_checksum_accumulator_ = 0;
  }
  struct FirmwareTransfer {
    drum::firmware::FirmwareImageMetadata metadata{};
    drum::firmware::PartitionRegion region{0U, 0U};
    bool has_region = false;
  };
  FirmwareTransfer firmware_transfer_;
  etl::array<uint8_t, 128> firmware_decode_buffer_{};
  uint32_t firmware_checksum_accumulator_;

  // Parse 14-bit number from 2 bytes (SDS format)
  static constexpr uint16_t parse_14bit(uint8_t low, uint8_t high) {
    return (static_cast<uint16_t>(low) & 0x7F) |
           ((static_cast<uint16_t>(high) & 0x7F) << 7);
  }

  // Parse 21-bit number from 3 bytes (SDS format)
  static constexpr uint32_t parse_21bit(uint8_t b0, uint8_t b1, uint8_t b2) {
    return (static_cast<uint32_t>(b0) & 0x7F) |
           ((static_cast<uint32_t>(b1) & 0x7F) << 7) |
           ((static_cast<uint32_t>(b2) & 0x7F) << 14);
  }

  // Unpack 16-bit sample from SDS 3-byte format
  static constexpr int16_t unpack_16bit_sample(uint8_t b0, uint8_t b1,
                                               uint8_t b2) {
    // Reconstruct unsigned 16-bit value (left-justified in 3 bytes)
    const uint16_t unsigned_sample = ((static_cast<uint16_t>(b0) & 0x7F) << 9) |
                                     ((static_cast<uint16_t>(b1) & 0x7F) << 2) |
                                     ((static_cast<uint16_t>(b2) & 0x7F) >> 5);

    // Convert from SDS unsigned format (0x0000 = full negative) to signed
    return static_cast<int16_t>(unsigned_sample - 0x8000);
  }

  // Calculate checksum for data packet validation
  static constexpr uint8_t
  calculate_checksum(uint8_t packet_num, const etl::span<const uint8_t> &data) {
    // XOR of: 0x7E (non-realtime), channel, 0x02 (data packet), packet_num, and
    // all data
    uint8_t checksum =
        0x7E ^ 0x65 ^ DATA_PACKET ^ packet_num; // 0x65 = DRUM channel
    for (const uint8_t byte : data) {
      checksum ^= byte;
    }
    return checksum & 0x7F;
  }

  // Handle Cancel message
  constexpr Result handle_cancel_message() {
    logger_.info("SDS: Transfer cancelled by host.");
    if (!is_busy()) {
      return Result::Cancelled;
    }

    if (transfer_type_ == TransferType::Firmware) {
      if (flash_writer_ != nullptr) {
        flash_writer_->cancel();
      }
      if (partition_manager_ != nullptr) {
        partition_manager_->abort_staging();
      }
      reset_firmware_state();
    }

    opened_file_.reset();
    transfer_type_ = TransferType::None;
    state_ = State::Idle;
    // No reply should be sent for a CANCEL message.
    return Result::Cancelled;
  }

  // Handle Dump Header message
  template <typename Sender>
  constexpr Result handle_dump_header(const etl::span<const uint8_t> &message,
                                      Sender send_reply,
                                      [[maybe_unused]] absolute_time_t now) {
    if (message.size() < 17) { // Minimum size for dump header
      logger_.error("SDS: Dump header too short:",
                    static_cast<uint32_t>(message.size()));
      send_reply(NAK, 0);
      return Result::InvalidMessage;
    }

    if (const auto firmware_metadata = parse_firmware_header(message)) {
      return handle_firmware_dump_header(*firmware_metadata, send_reply);
    }

    reset_firmware_state();

    // Parse header fields
    current_sample_.sample_number = parse_14bit(message[1], message[2]);
    current_sample_.bit_depth = message[3];
    current_sample_.sample_period_ns =
        parse_21bit(message[4], message[5], message[6]);
    current_sample_.length_words =
        parse_21bit(message[7], message[8], message[9]);
    current_sample_.loop_start =
        parse_21bit(message[10], message[11], message[12]);
    current_sample_.loop_end =
        parse_21bit(message[13], message[14], message[15]);
    current_sample_.loop_type = message[16];

    logger_.info("SDS: Dump Header received");
    logger_.info("Sample number:",
                 static_cast<uint32_t>(current_sample_.sample_number));
    logger_.info("Bit depth:",
                 static_cast<uint32_t>(current_sample_.bit_depth));
    logger_.info("Sample rate:", current_sample_.get_sample_rate());
    logger_.info("Length:", current_sample_.get_byte_length());

    // Validate parameters
    if (current_sample_.bit_depth != 16) {
      logger_.error("SDS: Only 16-bit samples supported, got:",
                    static_cast<uint32_t>(current_sample_.bit_depth));
      send_reply(NAK, 0);
      return Result::InvalidMessage;
    }

    if (current_sample_.get_byte_length() == 0) {
      logger_.error("SDS: Invalid sample length");
      send_reply(NAK, 0);
      return Result::InvalidMessage;
    }

    // Create filename from sample number
    char filename[16];
    snprintf(filename, sizeof(filename), "/%02u.pcm",
             current_sample_.sample_number);

    // Open file for writing
    opened_file_.emplace(file_ops_, filename);
    if (!opened_file_->is_valid()) {
      logger_.error("SDS: Failed to open file");
      send_reply(NAK, 0);
      return Result::FileError;
    }

    // Initialize receive state
    transfer_type_ = TransferType::Sample;
    state_ = State::ReceivingData;
    expected_packet_num_ = 0;
    bytes_received_ = 0;

    logger_.info("SDS: Ready to receive data packets");
    send_reply(ACK, 0);
    return Result::OK;
  }

  template <typename Sender>
  Result handle_firmware_dump_header(
      const drum::firmware::FirmwareImageMetadata &metadata,
      Sender send_reply) {
    logger_.info("SDS: Firmware Dump Header received");

    if (state_ != State::Idle) {
      logger_.error("SDS: Firmware header received while busy");
      send_reply(NAK, 0);
      return Result::StateError;
    }

    if (partition_manager_ == nullptr || flash_writer_ == nullptr) {
      logger_.error("SDS: Firmware handlers not attached");
      send_reply(NAK, 0);
      return Result::StateError;
    }

    reset_firmware_state();
    opened_file_.reset();

    const auto region = partition_manager_->begin_staging(metadata);
    if (!region.has_value()) {
      logger_.error("SDS: Unable to reserve firmware partition");
      send_reply(NAK, 0);
      return Result::PartitionError;
    }

    if (metadata.declared_size > region->length) {
      logger_.error("SDS: Firmware image exceeds target partition");
      partition_manager_->abort_staging();
      send_reply(NAK, 0);
      return Result::PartitionError;
    }

    if (!flash_writer_->begin(*region, metadata)) {
      logger_.error("SDS: Flash writer rejected begin");
      partition_manager_->abort_staging();
      send_reply(NAK, 0);
      return Result::FlashError;
    }

    firmware_transfer_.metadata = metadata;
    firmware_transfer_.region = *region;
    firmware_transfer_.has_region = true;
    transfer_type_ = TransferType::Firmware;
    state_ = State::ReceivingData;
    expected_packet_num_ = 0;
    bytes_received_ = 0;
    firmware_checksum_accumulator_ = 0;

    logger_.info("SDS: Ready to receive firmware data");
    send_reply(ACK, 0);
    return Result::OK;
  }

  // Handle Data Packet message
  template <typename Sender>
  constexpr Result handle_data_packet(const etl::span<const uint8_t> &message,
                                      Sender send_reply,
                                      [[maybe_unused]] absolute_time_t now) {
    if (state_ != State::ReceivingData) {
      logger_.error("SDS: Data packet received in wrong state");
      send_reply(NAK, 0);
      return Result::StateError;
    }

    if (transfer_type_ == TransferType::Firmware) {
      return handle_firmware_data_packet(message, send_reply);
    }

    if (transfer_type_ != TransferType::Sample) {
      logger_.error("SDS: Data packet without active transfer type");
      send_reply(NAK, 0);
      return Result::StateError;
    }

    if (message.size() != 123) { // 1 + 1 + 120 + 1 bytes
      logger_.error("SDS: Invalid data packet size:",
                    static_cast<uint32_t>(message.size()));
      send_reply(NAK, expected_packet_num_);
      return Result::InvalidMessage;
    }

    const uint8_t packet_num = message[1];
    const auto data_span = message.subspan(2, 120);
    const uint8_t received_checksum = message[122];

    // Verify checksum
    const uint8_t calculated_checksum =
        calculate_checksum(packet_num, data_span);
    if (received_checksum != calculated_checksum) {
      logger_.error("SDS: Checksum mismatch, expected:",
                    static_cast<uint32_t>(calculated_checksum));
      logger_.error("SDS: Checksum mismatch, got:",
                    static_cast<uint32_t>(received_checksum));
      send_reply(NAK, packet_num);
      return Result::ChecksumError;
    }

    // Check packet sequence
    if (packet_num != expected_packet_num_) {
      logger_.warn("SDS: Unexpected packet number, expected:",
                   static_cast<uint32_t>(expected_packet_num_));
      logger_.warn("SDS: Unexpected packet number, got:",
                   static_cast<uint32_t>(packet_num));
      // For now, accept out-of-order packets (could improve this)
    }

    // Unpack samples from data packet (40 samples, 3 bytes each)
    etl::array<uint8_t, 80> unpacked_data; // 40 samples * 2 bytes each
    size_t output_pos = 0;

    for (size_t i = 0; i < 40 && output_pos < unpacked_data.size(); i++) {
      const size_t data_offset = i * 3;
      if (data_offset + 2 < data_span.size()) {
        const int16_t sample = unpack_16bit_sample(data_span[data_offset],
                                                   data_span[data_offset + 1],
                                                   data_span[data_offset + 2]);

        // Write as little-endian 16-bit
        unpacked_data[output_pos++] = static_cast<uint8_t>(sample & 0xFF);
        unpacked_data[output_pos++] =
            static_cast<uint8_t>((sample >> 8) & 0xFF);
      }
    }

    // Determine how many bytes to actually write
    const uint32_t remaining_bytes =
        current_sample_.get_byte_length() - bytes_received_;
    const size_t bytes_to_write =
        etl::min(static_cast<size_t>(remaining_bytes), output_pos);

    // Write unpacked samples to file
    if (opened_file_ && opened_file_->is_valid()) {
      const size_t written = opened_file_->write(
          etl::span<const uint8_t>{unpacked_data.data(), bytes_to_write});

      if (written != bytes_to_write) {
        logger_.error("SDS: Failed to write sample data");
        opened_file_.reset();
        state_ = State::Idle;
        transfer_type_ = TransferType::None;
        send_reply(NAK, packet_num);
        return Result::FileError;
      }

      bytes_received_ += written;
    }

    // Update expected packet number (with wraparound)
    expected_packet_num_ = (packet_num + 1) & 0x7F;

    logger_.info("SDS: Packet received, bytes:", bytes_received_);

    // Check if transfer is complete
    if (bytes_received_ >= current_sample_.get_byte_length()) {
      logger_.info("SDS: Sample transfer complete");
      opened_file_.reset();
      state_ = State::Idle;
      transfer_type_ = TransferType::None;
      send_reply(ACK, packet_num);
      return Result::SampleComplete;
    }

    send_reply(ACK, packet_num);
    return Result::OK;
  }

  template <typename Sender>
  Result handle_firmware_data_packet(const etl::span<const uint8_t> &message,
                                     Sender send_reply) {
    if (message.size() != 123) {
      logger_.error("SDS: Invalid firmware packet size:",
                    static_cast<uint32_t>(message.size()));
      send_reply(NAK, expected_packet_num_);
      return Result::InvalidMessage;
    }

    if (!firmware_transfer_.has_region || flash_writer_ == nullptr ||
        partition_manager_ == nullptr) {
      logger_.error("SDS: Firmware data without active transfer");
      send_reply(NAK, 0);
      return Result::StateError;
    }

    const uint8_t packet_num = message[1];
    const auto data_span = message.subspan(2, 120);
    const uint8_t received_checksum = message[122];

    const uint8_t calculated_checksum =
        calculate_checksum(packet_num, data_span);
    if (received_checksum != calculated_checksum) {
      logger_.error("SDS: Firmware checksum mismatch, expected:",
                    static_cast<uint32_t>(calculated_checksum));
      logger_.error("SDS: Firmware checksum mismatch, got:",
                    static_cast<uint32_t>(received_checksum));
      send_reply(NAK, packet_num);
      return Result::ChecksumError;
    }

    if (packet_num != expected_packet_num_) {
      logger_.warn("SDS: Firmware packet out of order, expected:",
                   static_cast<uint32_t>(expected_packet_num_));
      logger_.warn("SDS: Firmware packet out of order, got:",
                   static_cast<uint32_t>(packet_num));
    }

    const auto decode_result = sysex::codec::decode_8_to_7(
        data_span.cbegin(), data_span.cend(), firmware_decode_buffer_.begin(),
        firmware_decode_buffer_.end());

    if (decode_result.first != data_span.size()) {
      logger_.error("SDS: Firmware packet decode mismatch");
      send_reply(NAK, packet_num);
      return Result::InvalidMessage;
    }

    if (bytes_received_ > firmware_transfer_.metadata.declared_size) {
      logger_.error("SDS: Firmware byte tracking mismatch");
      flash_writer_->cancel();
      partition_manager_->abort_staging();
      reset_firmware_state();
      transfer_type_ = TransferType::None;
      state_ = State::Idle;
      send_reply(NAK, packet_num);
      return Result::StateError;
    }

    const uint32_t remaining_bytes =
        firmware_transfer_.metadata.declared_size - bytes_received_;
    const size_t bytes_to_stage = decode_result.second;

    if (bytes_to_stage > remaining_bytes) {
      logger_.error("SDS: Firmware payload exceeds declared size");
      flash_writer_->cancel();
      partition_manager_->abort_staging();
      reset_firmware_state();
      transfer_type_ = TransferType::None;
      state_ = State::Idle;
      send_reply(NAK, packet_num);
      return Result::InvalidMessage;
    }

    if (bytes_to_stage > 0U) {
      const auto chunk = etl::span<const uint8_t>{
          firmware_decode_buffer_.data(), bytes_to_stage};
      if (!flash_writer_->write_chunk(chunk)) {
        logger_.error("SDS: Flash writer rejected chunk");
        flash_writer_->cancel();
        partition_manager_->abort_staging();
        reset_firmware_state();
        transfer_type_ = TransferType::None;
        state_ = State::Idle;
        send_reply(NAK, packet_num);
        return Result::FlashError;
      }

      for (size_t i = 0; i < bytes_to_stage; ++i) {
        firmware_checksum_accumulator_ += chunk[i];
      }

      bytes_received_ += static_cast<uint32_t>(bytes_to_stage);
    }

    expected_packet_num_ = (packet_num + 1U) & 0x7FU;

    if (bytes_received_ >= firmware_transfer_.metadata.declared_size) {
      if (!flash_writer_->finalize()) {
        logger_.error("SDS: Flash writer failed to finalize");
        flash_writer_->cancel();
        partition_manager_->abort_staging();
        reset_firmware_state();
        transfer_type_ = TransferType::None;
        state_ = State::Idle;
        send_reply(NAK, packet_num);
        return Result::FlashError;
      }

      const auto partition_result =
          partition_manager_->commit_staging(firmware_transfer_.metadata);
      if (partition_result != drum::firmware::PartitionError::None) {
        logger_.error("SDS: Firmware commit failed:",
                      static_cast<uint32_t>(partition_result));
        flash_writer_->cancel();
        partition_manager_->abort_staging();
        reset_firmware_state();
        transfer_type_ = TransferType::None;
        state_ = State::Idle;
        send_reply(NAK, packet_num);
        return Result::PartitionError;
      }

      reset_firmware_state();
      transfer_type_ = TransferType::None;
      state_ = State::Idle;
      send_reply(ACK, packet_num);
      return Result::FirmwareComplete;
    }

    send_reply(ACK, packet_num);
    return Result::OK;
  }
};

} // namespace sds

#endif // SDS_PROTOCOL_H_SDS_AUDIO

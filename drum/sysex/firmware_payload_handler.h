#ifndef DRUM_SYSEX_FIRMWARE_PAYLOAD_HANDLER_H
#define DRUM_SYSEX_FIRMWARE_PAYLOAD_HANDLER_H

/**
 * @file firmware_payload_handler.h
 * @brief Payload handler for firmware update transfers
 *
 * This class implements the payload handler interface for processing
 * firmware image transfers. It maintains exact compatibility with the
 * existing firmware update implementation.
 */

#include "drum/sysex/payload_handler.h"
#include "drum/sysex/codec.h"
#include "drum/firmware/update_interfaces.h"
#include "etl/array.h"
#include "etl/optional.h"
#include "etl/span.h"
#include "musin/hal/logger.h"

#include <cstdint>

namespace sysex {

/**
 * @brief Payload handler for firmware update transfers
 */
class FirmwarePayloadHandler {
public:
  constexpr FirmwarePayloadHandler(
      drum::firmware::FirmwarePartitionManager &partition_manager,
      drum::firmware::PartitionFlashWriter &flash_writer,
      musin::Logger &logger)
      : partition_manager_(partition_manager), flash_writer_(flash_writer),
        logger_(logger), bytes_received_(0), has_active_transfer_(false),
        firmware_checksum_accumulator_(0) {}

  /**
   * @brief Begin transfer with firmware dump header
   * @param header_data Complete firmware dump header
   * @return true if header valid and transfer can begin
   */
  constexpr bool begin_transfer(const etl::span<const uint8_t> &header_data) {
    const auto metadata = parse_firmware_header(header_data);
    if (!metadata.has_value()) {
      logger_.error("FirmwarePayload: Invalid firmware header");
      return false;
    }

    logger_.info("FirmwarePayload: Firmware Dump Header received");
    logger_.info("Format version:", static_cast<uint32_t>(metadata->format_version));
    logger_.info("Declared size:", metadata->declared_size);
    logger_.info("Version tag:", metadata->version_tag);

    // Cancel any existing transfer
    if (has_active_transfer_) {
      cancel_transfer();
    }

    const auto region = partition_manager_.begin_staging(*metadata);
    if (!region.has_value()) {
      logger_.error("FirmwarePayload: Unable to reserve firmware partition");
      return false;
    }

    if (metadata->declared_size > region->length) {
      logger_.error("FirmwarePayload: Firmware image exceeds target partition");
      partition_manager_.abort_staging();
      return false;
    }

    if (!flash_writer_.begin(*region, *metadata)) {
      logger_.error("FirmwarePayload: Flash writer rejected begin");
      partition_manager_.abort_staging();
      return false;
    }

    // Initialize transfer state
    metadata_ = *metadata;
    region_ = *region;
    has_active_transfer_ = true;
    bytes_received_ = 0;
    firmware_checksum_accumulator_ = 0;

    logger_.info("FirmwarePayload: Ready to receive firmware data");
    return true;
  }

  /**
   * @brief Process firmware data packet
   * @param packet_data Data payload (120 bytes of 7-bit encoded data)
   * @param packet_num Packet sequence number
   * @return Processing result
   */
  constexpr PayloadProcessResult process_packet(const etl::span<const uint8_t> &packet_data,
                                               uint8_t packet_num) {
    if (!has_active_transfer_) {
      logger_.error("FirmwarePayload: Data packet without active transfer");
      return PayloadProcessResult::Error;
    }

    if (packet_data.size() != 120) {
      logger_.error("FirmwarePayload: Invalid packet size:",
                    static_cast<uint32_t>(packet_data.size()));
      return PayloadProcessResult::Error;
    }

    // 7-bit decode the packet data (same as original implementation)
    const auto decode_result = codec::decode_8_to_7(
        packet_data.cbegin(), packet_data.cend(),
        firmware_decode_buffer_.begin(), firmware_decode_buffer_.end());

    if (decode_result.first != packet_data.size()) {
      logger_.error("FirmwarePayload: Packet decode mismatch");
      return PayloadProcessResult::Error;
    }

    // Validate byte tracking
    if (bytes_received_ > metadata_.declared_size) {
      logger_.error("FirmwarePayload: Byte tracking mismatch");
      return PayloadProcessResult::Error;
    }

    const uint32_t remaining_bytes = metadata_.declared_size - bytes_received_;
    const size_t bytes_to_stage = decode_result.second;

    if (bytes_to_stage > remaining_bytes) {
      logger_.error("FirmwarePayload: Payload exceeds declared size");
      return PayloadProcessResult::Error;
    }

    // Write decoded data to flash
    if (bytes_to_stage > 0U) {
      const auto chunk = etl::span<const uint8_t>{
          firmware_decode_buffer_.data(), bytes_to_stage};

      if (!flash_writer_.write_chunk(chunk)) {
        logger_.error("FirmwarePayload: Flash writer rejected chunk");
        return PayloadProcessResult::Error;
      }

      // Update checksum accumulator (same as original)
      for (size_t i = 0; i < bytes_to_stage; ++i) {
        firmware_checksum_accumulator_ += chunk[i];
      }

      bytes_received_ += static_cast<uint32_t>(bytes_to_stage);
    }

    logger_.info("FirmwarePayload: Packet processed, bytes:", bytes_received_);

    // Check if transfer is complete
    if (bytes_received_ >= metadata_.declared_size) {
      logger_.info("FirmwarePayload: Firmware transfer complete");
      return PayloadProcessResult::TransferComplete;
    }

    return PayloadProcessResult::OK;
  }

  /**
   * @brief Finalize completed transfer
   * @return true if finalization successful
   */
  constexpr bool finalize_transfer() {
    if (!has_active_transfer_) {
      return false;
    }

    if (!flash_writer_.finalize()) {
      logger_.error("FirmwarePayload: Flash writer failed to finalize");
      cancel_transfer();
      return false;
    }

    const auto partition_result = partition_manager_.commit_staging(metadata_);
    if (partition_result != drum::firmware::PartitionError::None) {
      logger_.error("FirmwarePayload: Firmware commit failed:",
                    static_cast<uint32_t>(partition_result));
      cancel_transfer();
      return false;
    }

    has_active_transfer_ = false;
    logger_.info("FirmwarePayload: Transfer finalized successfully");
    return true;
  }

  /**
   * @brief Cancel transfer and cleanup
   */
  constexpr void cancel_transfer() {
    if (has_active_transfer_) {
      flash_writer_.cancel();
      partition_manager_.abort_staging();
      has_active_transfer_ = false;
    }
    bytes_received_ = 0;
    firmware_checksum_accumulator_ = 0;
  }

  /**
   * @brief Calculate checksum for firmware data packet
   * @param packet_num Packet sequence number
   * @param data Packet data payload
   * @return 7-bit checksum value
   */
  constexpr uint8_t calculate_checksum(uint8_t packet_num,
                                      const etl::span<const uint8_t> &data) {
    // Same checksum algorithm as SDS (for compatibility when using DataTransferProtocol)
    uint8_t checksum = 0x7E ^ 0x65 ^ 0x02 ^ packet_num;
    for (const uint8_t byte : data) {
      checksum ^= byte;
    }
    return checksum & 0x7F;
  }

private:
  drum::firmware::FirmwarePartitionManager &partition_manager_;
  drum::firmware::PartitionFlashWriter &flash_writer_;
  musin::Logger &logger_;

  drum::firmware::FirmwareImageMetadata metadata_;
  drum::firmware::PartitionRegion region_;
  uint32_t bytes_received_;
  bool has_active_transfer_;
  uint32_t firmware_checksum_accumulator_;

  // Decode buffer for 7-bit to 8-bit conversion (same size as original)
  etl::array<uint8_t, 128> firmware_decode_buffer_;

  static constexpr uint16_t FIRMWARE_HEADER_TOKEN = 0x3FFF;

  // Parse 14-bit number from 2 bytes (same as original)
  static constexpr uint16_t parse_14bit(uint8_t low, uint8_t high) {
    return (static_cast<uint16_t>(low) & 0x7F) |
           ((static_cast<uint16_t>(high) & 0x7F) << 7);
  }

  // Parse 21-bit number from 3 bytes (same as original)
  static constexpr uint32_t parse_21bit(uint8_t b0, uint8_t b1, uint8_t b2) {
    return (static_cast<uint32_t>(b0) & 0x7F) |
           ((static_cast<uint32_t>(b1) & 0x7F) << 7) |
           ((static_cast<uint32_t>(b2) & 0x7F) << 14);
  }

  // Combine checksum fields (same as original)
  static constexpr uint32_t combine_checksum_fields(uint32_t high21, uint32_t low21) {
    const uint32_t high_bits = high21 & 0x7FFu; // lower 11 bits
    return (high_bits << 21) | (low21 & 0x1FFFFFu);
  }

  // Parse firmware header from dump header message (same logic as original)
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
};

} // namespace sysex

#endif // DRUM_SYSEX_FIRMWARE_PAYLOAD_HANDLER_H
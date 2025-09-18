#ifndef DRUM_SYSEX_FIRMWARE_UPDATE_PROTOCOL_H
#define DRUM_SYSEX_FIRMWARE_UPDATE_PROTOCOL_H

/**
 * @file firmware_update_protocol.h
 * @brief Dedicated firmware update protocol using manufacturer-specific SysEx
 *
 * This protocol handles firmware updates using proper manufacturer-specific
 * SysEx messages instead of overloading the MIDI Sample Dump Standard.
 * It provides a clean separation between audio sample transfers and firmware
 * updates while maintaining the same reliable transfer mechanism.
 */

#include "drum/sysex/data_transfer_protocol.h"
#include "drum/sysex/firmware_payload_handler.h"
#include "drum/firmware/update_interfaces.h"
#include "etl/span.h"
#include "musin/hal/logger.h"

extern "C" {
#include "pico/time.h"
}

#include <cstdint>

namespace drum::firmware {

/**
 * @brief Firmware update protocol message types
 */
enum class MessageType : uint8_t {
  FirmwareUpdateHeader = 0x10,  ///< Begin firmware update with metadata
  FirmwareData = 0x11,          ///< Firmware data packet
  FirmwareCancel = 0x12,        ///< Cancel firmware update
  FirmwareAck = 0x13,          ///< Acknowledgment
  FirmwareNak = 0x14           ///< Negative acknowledgment
};

/**
 * @brief Firmware update protocol states
 */
enum class UpdateState {
  Idle,              ///< No active update
  ReceivingFirmware  ///< Receiving firmware data
};

/**
 * @brief Firmware update protocol results
 */
enum class UpdateResult {
  OK,                ///< Operation successful
  UpdateComplete,    ///< Firmware update completed successfully
  Cancelled,         ///< Update cancelled by sender
  InvalidMessage,    ///< Malformed message received
  ChecksumError,     ///< Packet checksum validation failed
  StateError,        ///< Operation not valid in current state
  FlashError,        ///< Flash programming error
  PartitionError     ///< Partition management error
};

/**
 * @brief Dedicated firmware update protocol
 */
class FirmwareUpdateProtocol {
public:
  constexpr FirmwareUpdateProtocol(
      drum::firmware::FirmwarePartitionManager &partition_manager,
      drum::firmware::PartitionFlashWriter &flash_writer,
      musin::Logger &logger)
      : firmware_handler_(partition_manager, flash_writer, logger),
        data_transfer_protocol_(firmware_handler_, logger) {}

  /**
   * @brief Process incoming firmware update message
   * @param message_type Firmware message type
   * @param message_data Message payload data
   * @param send_response Function to send response messages
   * @param now Current timestamp
   * @return Update result
   */
  template <typename Sender>
  constexpr UpdateResult process_message(uint8_t message_type,
                                        const etl::span<const uint8_t> &message_data,
                                        Sender send_response,
                                        absolute_time_t now) {
    // Convert firmware message type to standard transfer protocol type
    uint8_t transfer_message_type;
    switch (static_cast<MessageType>(message_type)) {
    case MessageType::FirmwareUpdateHeader:
      transfer_message_type = 0x01; // DUMP_HEADER equivalent
      break;
    case MessageType::FirmwareData:
      transfer_message_type = 0x02; // DATA_PACKET equivalent
      break;
    case MessageType::FirmwareCancel:
      transfer_message_type = 0x7D; // CANCEL equivalent
      break;
    default:
      send_response(static_cast<uint8_t>(MessageType::FirmwareNak), 0);
      return UpdateResult::InvalidMessage;
    }

    // Convert firmware response sender to DataTransferProtocol format
    auto protocol_sender = [send_response](uint8_t response_type, uint8_t packet_num) {
      // Map DataTransferProtocol responses to firmware message types
      const MessageType firmware_response = (response_type == 0x7F) ?
          MessageType::FirmwareAck : MessageType::FirmwareNak;
      send_response(static_cast<uint8_t>(firmware_response), packet_num);
    };

    // Delegate to DataTransferProtocol
    const auto transfer_result = data_transfer_protocol_.process_message(
        transfer_message_type, message_data, protocol_sender, now);

    // Convert DataTransferProtocol results to firmware update results
    return map_transfer_result(transfer_result);
  }

  /**
   * @brief Get current update state
   */
  constexpr UpdateState get_state() const {
    const auto transfer_state = data_transfer_protocol_.get_state();
    switch (transfer_state) {
    case sysex::TransferState::Idle:
      return UpdateState::Idle;
    case sysex::TransferState::ReceivingHeader:
    case sysex::TransferState::ReceivingData:
      return UpdateState::ReceivingFirmware;
    default:
      return UpdateState::Idle;
    }
  }

  /**
   * @brief Check if update is in progress
   */
  constexpr bool is_busy() const {
    return data_transfer_protocol_.is_busy();
  }

  /**
   * @brief Cancel any active firmware update
   */
  constexpr void cancel_update() {
    firmware_handler_.cancel_transfer();
  }

private:
  sysex::FirmwarePayloadHandler firmware_handler_;
  sysex::DataTransferProtocol<sysex::FirmwarePayloadHandler> data_transfer_protocol_;

  /**
   * @brief Map DataTransferProtocol results to firmware update results
   */
  constexpr UpdateResult map_transfer_result(sysex::TransferResult transfer_result) const {
    switch (transfer_result) {
    case sysex::TransferResult::OK:
      return UpdateResult::OK;
    case sysex::TransferResult::TransferComplete:
      return UpdateResult::UpdateComplete;
    case sysex::TransferResult::Cancelled:
      return UpdateResult::Cancelled;
    case sysex::TransferResult::InvalidMessage:
      return UpdateResult::InvalidMessage;
    case sysex::TransferResult::ChecksumError:
      return UpdateResult::ChecksumError;
    case sysex::TransferResult::StateError:
      return UpdateResult::StateError;
    case sysex::TransferResult::PayloadError:
      // Distinguish between flash and partition errors based on handler state
      // For now, map to flash error (could be enhanced to check specific error)
      return UpdateResult::FlashError;
    default:
      return UpdateResult::InvalidMessage;
    }
  }
};

/**
 * @brief Firmware update message builder utilities
 */
namespace message_builder {

/**
 * @brief Create firmware update header message
 * @param metadata Firmware image metadata
 * @return Header message payload
 */
constexpr etl::array<uint8_t, 17> create_firmware_header(
    const FirmwareImageMetadata &metadata) {
  etl::array<uint8_t, 17> header;

  // Use firmware-specific header format with special token
  header[0] = static_cast<uint8_t>(MessageType::FirmwareUpdateHeader);

  // Pack firmware header token (0x3FFF) in sample number field
  header[1] = 0x7F; // Low 7 bits of 0x3FFF
  header[2] = 0x7F; // High 7 bits of 0x3FFF

  header[3] = metadata.format_version & 0x7F;

  // Pack declared size (21-bit)
  header[4] = static_cast<uint8_t>(metadata.declared_size & 0x7F);
  header[5] = static_cast<uint8_t>((metadata.declared_size >> 7) & 0x7F);
  header[6] = static_cast<uint8_t>((metadata.declared_size >> 14) & 0x7F);

  // Pack checksum fields (split 32-bit into two 21-bit values)
  const uint32_t checksum_low = metadata.checksum & 0x1FFFFF;
  const uint32_t checksum_high = (metadata.checksum >> 21) & 0x7FF;

  header[7] = static_cast<uint8_t>(checksum_high & 0x7F);
  header[8] = static_cast<uint8_t>((checksum_high >> 7) & 0x7F);
  header[9] = static_cast<uint8_t>((checksum_high >> 14) & 0x7F);

  header[10] = static_cast<uint8_t>(checksum_low & 0x7F);
  header[11] = static_cast<uint8_t>((checksum_low >> 7) & 0x7F);
  header[12] = static_cast<uint8_t>((checksum_low >> 14) & 0x7F);

  // Pack version tag (21-bit)
  header[13] = static_cast<uint8_t>(metadata.version_tag & 0x7F);
  header[14] = static_cast<uint8_t>((metadata.version_tag >> 7) & 0x7F);
  header[15] = static_cast<uint8_t>((metadata.version_tag >> 14) & 0x7F);

  header[16] = metadata.partition_hint & 0x7F;

  return header;
}

} // namespace message_builder

} // namespace drum::firmware

#endif // DRUM_SYSEX_FIRMWARE_UPDATE_PROTOCOL_H
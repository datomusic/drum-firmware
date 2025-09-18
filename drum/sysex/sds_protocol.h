#ifndef SDS_PROTOCOL_H_SDS_AUDIO
#define SDS_PROTOCOL_H_SDS_AUDIO

/**
 * @file sds_protocol.h
 * @brief Refactored MIDI Sample Dump Standard (SDS) protocol implementation
 *
 * This implements a clean SDS specification for receiving 16-bit PCM audio
 * samples only. All firmware-related functionality has been removed and
 * moved to a dedicated FirmwareUpdateProtocol.
 *
 * Supported features:
 * - Dump Header parsing with basic sample metadata
 * - Data Packet processing with 16-bit sample unpacking
 * - ACK/NAK response generation
 * - Checksum validation
 * - Integration with existing file operations
 */

#include "drum/sysex/data_transfer_protocol.h"
#include "drum/sysex/sample_payload_handler.h"
#include "etl/span.h"
#include "musin/hal/logger.h"

extern "C" {
#include "pico/time.h"
}

#include <cstdint>

namespace sds {

// SDS Message Types (unchanged from original)
enum MessageType : uint8_t {
  DUMP_HEADER = 0x01,
  DATA_PACKET = 0x02,
  DUMP_REQUEST = 0x03,
  ACK = 0x7F,
  NAK = 0x7E,
  CANCEL = 0x7D,
  WAIT = 0x7C
};

// SDS Protocol State (simplified - no firmware state)
enum class State {
  Idle,
  ReceivingData
};

// SDS Protocol Result (simplified - no firmware results)
enum class Result {
  OK,
  SampleComplete,
  Cancelled,
  InvalidMessage,
  ChecksumError,
  FileError,
  StateError
};

/**
 * @brief SDS protocol implementation using DataTransferProtocol
 * @tparam FileOperations File operations interface type
 */
template <typename FileOperations> class Protocol {
public:
  constexpr Protocol(FileOperations &file_ops, musin::Logger &logger)
      : sample_handler_(file_ops, logger),
        data_transfer_protocol_(sample_handler_, logger) {
  }

  /**
   * @brief Process incoming SDS message
   * @param message Complete SDS message payload (without SysEx framing)
   * @param send_reply Function to send SDS response messages
   * @param now Current timestamp
   * @return SDS result
   */
  template <typename Sender>
  Result process_message(const etl::span<const uint8_t> &message,
                         const Sender &send_reply, absolute_time_t now) {
    if (message.size() < 1) {
      return Result::InvalidMessage;
    }

    const uint8_t message_type = message[0];

    // Pre-validate SDS-specific constraints before delegating
    if (message_type == DUMP_HEADER && message.size() >= 4) {
      const uint8_t bit_depth =
          message[3]; // Bit depth is at offset 3 in original message
      if (bit_depth != 16) {
        send_reply(NAK, 0);
        return Result::InvalidMessage;
      }
    }

    // Convert SDS response sender to DataTransferProtocol format
    auto protocol_sender = [&send_reply](uint8_t response_type,
                                         uint8_t packet_num) {
      // Map DataTransferProtocol responses to SDS message types
      const MessageType sds_response = (response_type == 0x7F) ? ACK : NAK;
      send_reply(sds_response, packet_num);
    };

    // Strip message type from payload before delegating to DataTransferProtocol
    const auto message_payload = message.subspan(1);
    const auto transfer_result = data_transfer_protocol_.process_message(
        message_type, message_payload, protocol_sender, now);

    // Convert DataTransferProtocol results to SDS results
    switch (transfer_result) {
    case sysex::TransferResult::OK:
      return Result::OK;
    case sysex::TransferResult::TransferComplete:
      return Result::SampleComplete;
    case sysex::TransferResult::Cancelled:
      return Result::Cancelled;
    case sysex::TransferResult::InvalidMessage:
      return Result::InvalidMessage;
    case sysex::TransferResult::ChecksumError:
      return Result::ChecksumError;
    case sysex::TransferResult::StateError:
      return Result::StateError;
    case sysex::TransferResult::PayloadError:
      return Result::FileError; // Map payload errors to file errors for SDS
    default:
      return Result::InvalidMessage;
    }
  }

  /**
   * @brief Get current protocol state
   */
  State get_state() const {
    const auto transfer_state = data_transfer_protocol_.get_state();
    switch (transfer_state) {
    case sysex::TransferState::Idle:
      return State::Idle;
    case sysex::TransferState::ReceivingHeader:
    case sysex::TransferState::ReceivingData:
      return State::ReceivingData;
    default:
      return State::Idle;
    }
  }

  /**
   * @brief Check if protocol is busy
   */
  bool is_busy() const {
    return data_transfer_protocol_.is_busy();
  }

private:
  SamplePayloadHandler<FileOperations> sample_handler_;
  sysex::DataTransferProtocol<SamplePayloadHandler<FileOperations>>
      data_transfer_protocol_;
};

} // namespace sds

#endif // SDS_PROTOCOL_H_SDS_AUDIO
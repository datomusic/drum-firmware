#ifndef DRUM_SYSEX_DATA_TRANSFER_PROTOCOL_H
#define DRUM_SYSEX_DATA_TRANSFER_PROTOCOL_H

/**
 * @file data_transfer_protocol.h
 * @brief Generic data transfer protocol for reliable packet-based transfers
 *
 * This template class provides the common transport layer for both SDS sample
 * transfers and firmware updates. It handles:
 * - Packet sequencing and acknowledgment
 * - Checksum validation
 * - Session state management
 * - Error handling and recovery
 *
 * The actual data processing is delegated to payload handler classes that
 * implement the PayloadHandler concept.
 */

#include "etl/optional.h"
#include "etl/span.h"
#include "musin/hal/logger.h"
#include "drum/sysex/payload_handler.h"

extern "C" {
#include "pico/time.h"
}

#include <cstdint>

namespace sysex {

/**
 * @brief Transfer protocol state
 */
enum class TransferState {
  Idle,           ///< No active transfer
  ReceivingHeader, ///< Waiting for/processing transfer header
  ReceivingData   ///< Receiving data packets
};

/**
 * @brief Transfer protocol results
 */
enum class TransferResult {
  OK,              ///< Operation successful, continue
  TransferComplete, ///< Transfer completed successfully
  Cancelled,       ///< Transfer cancelled by sender
  InvalidMessage,  ///< Malformed message received
  ChecksumError,   ///< Packet checksum validation failed
  StateError,      ///< Operation not valid in current state
  PayloadError     ///< Payload handler reported error
};

/**
 * @brief Payload handler concept
 *
 * PayloadHandler classes must implement:
 * - begin_transfer(header_data) -> bool
 * - process_packet(packet_data, packet_num) -> bool
 * - finalize_transfer() -> bool
 * - cancel_transfer() -> void
 * - calculate_checksum(packet_num, data) -> uint8_t
 */

/**
 * @brief Generic data transfer protocol template
 *
 * @tparam PayloadHandler Type that handles transfer-specific data processing
 * @tparam Sender Function/callable for sending responses
 */
template <typename PayloadHandler>
class DataTransferProtocol {
public:
  /**
   * @brief Constructor
   * @param payload_handler Reference to payload processing handler
   * @param logger Reference to logger instance
   */
  constexpr DataTransferProtocol(PayloadHandler &payload_handler,
                                musin::Logger &logger)
      : payload_handler_(payload_handler), logger_(logger),
        state_(TransferState::Idle), expected_packet_num_(0) {}

  /**
   * @brief Process incoming transfer message
   * @param message_type Message type identifier
   * @param message_data Message payload data
   * @param send_response Function to send response messages
   * @param now Current timestamp
   * @return Transfer result
   */
  template <typename Sender>
  TransferResult process_message(uint8_t message_type,
                                const etl::span<const uint8_t> &message_data,
                                Sender send_response,
                                absolute_time_t now) {
    // Handle message based on type
    switch (message_type) {
    case 0x01: // DUMP_HEADER
      return handle_dump_header(message_data, send_response, now);
    case 0x02: // DATA_PACKET
      return handle_data_packet(message_data, send_response, now);
    case 0x7D: // CANCEL
      return handle_cancel_message();
    default:
      logger_.warn("DataTransfer: Unknown message type:",
                   static_cast<uint32_t>(message_type));
      send_response(0x7E, 0); // NAK
      return TransferResult::InvalidMessage;
    }
  }

  /**
   * @brief Get current transfer state
   */
  constexpr TransferState get_state() const {
    return state_;
  }

  /**
   * @brief Check if transfer is in progress
   */
  constexpr bool is_busy() const {
    return state_ != TransferState::Idle;
  }

private:
  PayloadHandler &payload_handler_;
  musin::Logger &logger_;
  TransferState state_;
  uint8_t expected_packet_num_;

  /**
   * @brief Handle dump header message
   */
  template <typename Sender>
  TransferResult handle_dump_header(const etl::span<const uint8_t> &message,
                                   Sender send_response,
                                   absolute_time_t now) {
    if (message.size() < 17) { // Minimum size for dump header
      logger_.error("DataTransfer: Dump header too short:",
                    static_cast<uint32_t>(message.size()));
      send_response(0x7E, 0); // NAK
      return TransferResult::InvalidMessage;
    }

    // Cancel any existing transfer
    if (state_ != TransferState::Idle) {
      logger_.warn("DataTransfer: New header received during active transfer, cancelling previous");
      payload_handler_.cancel_transfer();
    }

    // Delegate header processing to payload handler
    if (!payload_handler_.begin_transfer(message)) {
      logger_.error("DataTransfer: Payload handler rejected header");
      state_ = TransferState::Idle;
      send_response(0x7E, 0); // NAK
      return TransferResult::PayloadError;
    }

    // Initialize transfer state
    state_ = TransferState::ReceivingData;
    expected_packet_num_ = 0;

    logger_.info("DataTransfer: Header accepted, ready for data");
    send_response(0x7F, 0); // ACK
    return TransferResult::OK;
  }

  /**
   * @brief Handle data packet message
   */
  template <typename Sender>
  TransferResult handle_data_packet(const etl::span<const uint8_t> &message,
                                   Sender send_response,
                                   absolute_time_t now) {
    if (state_ != TransferState::ReceivingData) {
      logger_.error("DataTransfer: Data packet received in wrong state");
      send_response(0x7E, 0); // NAK
      return TransferResult::StateError;
    }

    if (message.size() != 123) { // Standard data packet size
      logger_.error("DataTransfer: Invalid data packet size:",
                    static_cast<uint32_t>(message.size()));
      send_response(0x7E, expected_packet_num_); // NAK with expected packet
      return TransferResult::InvalidMessage;
    }

    const uint8_t packet_num = message[1];
    const auto data_span = message.subspan(2, 120);
    const uint8_t received_checksum = message[122];

    // Validate checksum using payload handler's algorithm
    const uint8_t calculated_checksum =
        payload_handler_.calculate_checksum(packet_num, data_span);
    if (received_checksum != calculated_checksum) {
      logger_.error("DataTransfer: Checksum mismatch, expected:",
                    static_cast<uint32_t>(calculated_checksum));
      logger_.error("DataTransfer: Checksum mismatch, got:",
                    static_cast<uint32_t>(received_checksum));
      send_response(0x7E, packet_num); // NAK
      return TransferResult::ChecksumError;
    }

    // Check packet sequence (log warning but accept out-of-order)
    if (packet_num != expected_packet_num_) {
      logger_.warn("DataTransfer: Unexpected packet number, expected:",
                   static_cast<uint32_t>(expected_packet_num_));
      logger_.warn("DataTransfer: Unexpected packet number, got:",
                   static_cast<uint32_t>(packet_num));
    }

    // Delegate packet processing to payload handler
    const auto process_result = payload_handler_.process_packet(data_span, packet_num);
    if (process_result == PayloadProcessResult::Error) {
      logger_.error("DataTransfer: Payload handler failed to process packet");
      payload_handler_.cancel_transfer();
      state_ = TransferState::Idle;
      send_response(0x7E, packet_num); // NAK
      return TransferResult::PayloadError;
    }

    // Update expected packet number (with wraparound)
    expected_packet_num_ = (packet_num + 1) & 0x7F;

    // Check if transfer is complete
    if (process_result == PayloadProcessResult::TransferComplete) {
      if (!payload_handler_.finalize_transfer()) {
        logger_.error("DataTransfer: Payload handler failed to finalize");
        payload_handler_.cancel_transfer();
        state_ = TransferState::Idle;
        send_response(0x7E, packet_num); // NAK
        return TransferResult::PayloadError;
      }

      logger_.info("DataTransfer: Transfer completed successfully");
      state_ = TransferState::Idle;
      send_response(0x7F, packet_num); // ACK
      return TransferResult::TransferComplete;
    }

    // Continue receiving
    send_response(0x7F, packet_num); // ACK
    return TransferResult::OK;
  }

  /**
   * @brief Handle cancel message
   */
  constexpr TransferResult handle_cancel_message() {
    logger_.info("DataTransfer: Transfer cancelled by sender");

    if (state_ != TransferState::Idle) {
      payload_handler_.cancel_transfer();
      state_ = TransferState::Idle;
    }

    // No reply sent for cancel messages per SDS standard
    return TransferResult::Cancelled;
  }
};

} // namespace sysex

#endif // DRUM_SYSEX_DATA_TRANSFER_PROTOCOL_H
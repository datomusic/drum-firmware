#ifndef DRUM_SYSEX_PAYLOAD_HANDLER_H
#define DRUM_SYSEX_PAYLOAD_HANDLER_H

/**
 * @file payload_handler.h
 * @brief Payload handler interface and common types for data transfer protocol
 *
 * This file defines the interface that payload handlers must implement to
 * work with the DataTransferProtocol template class.
 */

#include "etl/span.h"
#include <cstdint>

namespace sysex {

/**
 * @brief Result codes for payload processing operations
 */
enum class PayloadProcessResult {
  OK,               ///< Packet processed successfully, continue transfer
  TransferComplete, ///< Transfer completed with this packet
  Error             ///< Error processing packet, abort transfer
};

/**
 * @brief Payload handler interface concept
 *
 * Classes implementing this concept must provide:
 * - begin_transfer(header_data) -> bool
 * - process_packet(packet_data, packet_num) -> PayloadProcessResult
 * - finalize_transfer() -> bool
 * - cancel_transfer() -> void
 * - calculate_checksum(packet_num, data) -> uint8_t
 *
 * This is a concept-based interface rather than virtual inheritance
 * to maintain constexpr compatibility and zero runtime overhead.
 */

/**
 * @brief Example payload handler implementation showing required interface
 *
 * This class serves as documentation for the payload handler concept.
 * Actual implementations should follow this pattern.
 */
class ExamplePayloadHandler {
public:
  /**
   * @brief Begin a new transfer with the given header data
   * @param header_data Complete header message payload
   * @return true if header is valid and transfer can begin, false otherwise
   */
  constexpr bool
  begin_transfer([[maybe_unused]] const etl::span<const uint8_t> &header_data) {
    // Parse header, validate parameters, initialize resources
    // Return false if header is invalid or resources unavailable
    return true;
  }

  /**
   * @brief Process a data packet
   * @param packet_data Data payload (120 bytes)
   * @param packet_num Packet sequence number (0-127)
   * @return Processing result
   */
  constexpr PayloadProcessResult
  process_packet([[maybe_unused]] const etl::span<const uint8_t> &packet_data,
                 [[maybe_unused]] uint8_t packet_num) {
    // Process packet data according to transfer type
    // Return TransferComplete when all expected data received
    // Return Error if processing fails
    return PayloadProcessResult::OK;
  }

  /**
   * @brief Finalize completed transfer
   * @return true if finalization successful, false if error
   */
  constexpr bool finalize_transfer() {
    // Flush buffers, close files, commit changes, etc.
    return true;
  }

  /**
   * @brief Cancel transfer and cleanup resources
   */
  constexpr void cancel_transfer() {
    // Abort operation, cleanup resources, reset state
  }

  /**
   * @brief Calculate checksum for data packet validation
   * @param packet_num Packet sequence number
   * @param data Packet data payload
   * @return 7-bit checksum value
   */
  constexpr uint8_t calculate_checksum(uint8_t packet_num,
                                       const etl::span<const uint8_t> &data) {
    // Implementation-specific checksum algorithm
    // For SDS: XOR of 0x7E ^ 0x65 ^ 0x02 ^ packet_num ^ all_data_bytes
    // For firmware: May use different algorithm
    uint8_t checksum = 0x7E ^ 0x65 ^ 0x02 ^ packet_num;
    for (const uint8_t byte : data) {
      checksum ^= byte;
    }
    return checksum & 0x7F;
  }
};

} // namespace sysex

#endif // DRUM_SYSEX_PAYLOAD_HANDLER_H
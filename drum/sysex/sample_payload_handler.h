#ifndef DRUM_SYSEX_SAMPLE_PAYLOAD_HANDLER_H
#define DRUM_SYSEX_SAMPLE_PAYLOAD_HANDLER_H

/**
 * @file sample_payload_handler.h
 * @brief Payload handler for SDS 16-bit PCM audio sample transfers
 *
 * This class implements the payload handler interface for processing
 * MIDI Sample Dump Standard (SDS) audio sample transfers. It maintains
 * exact compatibility with the existing SDS implementation.
 */

#include "drum/sysex/payload_handler.h"
#include "etl/array.h"
#include "etl/optional.h"
#include "etl/span.h"
#include "etl/string_view.h"
#include "musin/hal/logger.h"

#include <cstdint>
#include <cstdio>

namespace sds {

/**
 * @brief Sample metadata parsed from SDS dump header
 */
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

/**
 * @brief Payload handler for SDS 16-bit PCM sample transfers
 * @tparam FileOperations File operations interface type
 */
template <typename FileOperations>
class SamplePayloadHandler {
public:
  constexpr SamplePayloadHandler(FileOperations &file_ops, musin::Logger &logger)
      : file_ops_(file_ops), logger_(logger), bytes_received_(0) {}

  /**
   * @brief Begin transfer with SDS dump header
   * @param header_data Complete SDS dump header (17+ bytes)
   * @return true if header valid and transfer can begin
   */
  constexpr bool begin_transfer(const etl::span<const uint8_t> &header_data) {
    if (header_data.size() < 17) {
      logger_.error("SamplePayload: Header too short:",
                    static_cast<uint32_t>(header_data.size()));
      return false;
    }

    // Parse SDS dump header fields
    current_sample_.sample_number = parse_14bit(header_data[1], header_data[2]);
    current_sample_.bit_depth = header_data[3];
    current_sample_.sample_period_ns =
        parse_21bit(header_data[4], header_data[5], header_data[6]);
    current_sample_.length_words =
        parse_21bit(header_data[7], header_data[8], header_data[9]);
    current_sample_.loop_start =
        parse_21bit(header_data[10], header_data[11], header_data[12]);
    current_sample_.loop_end =
        parse_21bit(header_data[13], header_data[14], header_data[15]);
    current_sample_.loop_type = header_data[16];

    logger_.info("SamplePayload: Dump Header received");
    logger_.info("Sample number:",
                 static_cast<uint32_t>(current_sample_.sample_number));
    logger_.info("Bit depth:",
                 static_cast<uint32_t>(current_sample_.bit_depth));
    logger_.info("Sample rate:", current_sample_.get_sample_rate());
    logger_.info("Length:", current_sample_.get_byte_length());

    // Validate parameters (bit depth validation done at SDS protocol level)
    if (current_sample_.get_byte_length() == 0) {
      logger_.error("SamplePayload: Invalid sample length");
      return false;
    }

    // Create filename from sample number (same format as original)
    char filename[16];
    snprintf(filename, sizeof(filename), "/%02u.pcm",
             current_sample_.sample_number);

    // Open file for writing
    opened_file_.emplace(file_ops_, filename);
    if (!opened_file_ || !opened_file_->is_valid()) {
      logger_.error("SamplePayload: Failed to open file");
      opened_file_.reset();
      return false;
    }

    // Initialize transfer state
    bytes_received_ = 0;

    logger_.info("SamplePayload: Ready to receive data packets");
    return true;
  }

  /**
   * @brief Process SDS data packet
   * @param packet_data Data payload (120 bytes containing up to 40 samples)
   * @param packet_num Packet sequence number
   * @return Processing result
   */
  constexpr sysex::PayloadProcessResult process_packet(const etl::span<const uint8_t> &packet_data,
                                                      uint8_t packet_num) {
    if (!opened_file_ || !opened_file_->is_valid()) {
      logger_.error("SamplePayload: No file open for data packet");
      return sysex::PayloadProcessResult::Error;
    }

    if (packet_data.size() != 120) {
      logger_.error("SamplePayload: Invalid data packet size:",
                    static_cast<uint32_t>(packet_data.size()));
      return sysex::PayloadProcessResult::Error;
    }

    // Unpack samples from data packet (40 samples max, 3 bytes each)
    etl::array<uint8_t, 80> unpacked_data; // 40 samples * 2 bytes each
    size_t output_pos = 0;

    for (size_t i = 0; i < 40 && output_pos < unpacked_data.size(); i++) {
      const size_t data_offset = i * 3;
      if (data_offset + 2 < packet_data.size()) {
        const int16_t sample = unpack_16bit_sample(packet_data[data_offset],
                                                   packet_data[data_offset + 1],
                                                   packet_data[data_offset + 2]);

        // Write as little-endian 16-bit (same as original)
        unpacked_data[output_pos++] = static_cast<uint8_t>(sample & 0xFF);
        unpacked_data[output_pos++] =
            static_cast<uint8_t>((sample >> 8) & 0xFF);
      }
    }

    // Determine how many bytes to actually write (handle partial last packet)
    const uint32_t remaining_bytes =
        current_sample_.get_byte_length() - bytes_received_;
    const size_t bytes_to_write =
        etl::min(static_cast<size_t>(remaining_bytes), output_pos);

    // Write unpacked samples to file
    const size_t written = opened_file_->write(
        etl::span<const uint8_t>{unpacked_data.data(), bytes_to_write});

    if (written != bytes_to_write) {
      logger_.error("SamplePayload: Failed to write sample data");
      return sysex::PayloadProcessResult::Error;
    }

    bytes_received_ += written;

    logger_.info("SamplePayload: Packet processed, bytes:", bytes_received_);

    // Check if transfer is complete
    if (bytes_received_ >= current_sample_.get_byte_length()) {
      logger_.info("SamplePayload: Sample transfer complete");
      return sysex::PayloadProcessResult::TransferComplete;
    }

    return sysex::PayloadProcessResult::OK;
  }

  /**
   * @brief Finalize completed transfer
   * @return true if finalization successful
   */
  constexpr bool finalize_transfer() {
    if (opened_file_) {
      opened_file_.reset(); // Close file
    }
    return true;
  }

  /**
   * @brief Cancel transfer and cleanup
   */
  constexpr void cancel_transfer() {
    if (opened_file_) {
      opened_file_.reset(); // Close and abandon file
    }
    bytes_received_ = 0;
  }

  /**
   * @brief Calculate SDS checksum for data packet
   * @param packet_num Packet sequence number
   * @param data Packet data payload
   * @return 7-bit checksum value
   */
  constexpr uint8_t calculate_checksum(uint8_t packet_num,
                                      const etl::span<const uint8_t> &data) {
    // XOR of: 0x7E (non-realtime), 0x65 (DRUM channel), 0x02 (data packet), packet_num, and all data
    uint8_t checksum = 0x7E ^ 0x65 ^ 0x02 ^ packet_num;
    for (const uint8_t byte : data) {
      checksum ^= byte;
    }
    return checksum & 0x7F;
  }

private:
  FileOperations &file_ops_;
  musin::Logger &logger_;
  SampleInfo current_sample_;
  uint32_t bytes_received_;

  // File handle wrapper (same pattern as existing implementation)
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

  // Parse 14-bit number from 2 bytes (SDS format) - exact copy from original
  static constexpr uint16_t parse_14bit(uint8_t low, uint8_t high) {
    return (static_cast<uint16_t>(low) & 0x7F) |
           ((static_cast<uint16_t>(high) & 0x7F) << 7);
  }

  // Parse 21-bit number from 3 bytes (SDS format) - exact copy from original
  static constexpr uint32_t parse_21bit(uint8_t b0, uint8_t b1, uint8_t b2) {
    return (static_cast<uint32_t>(b0) & 0x7F) |
           ((static_cast<uint32_t>(b1) & 0x7F) << 7) |
           ((static_cast<uint32_t>(b2) & 0x7F) << 14);
  }

  // Unpack 16-bit sample from SDS 3-byte format - exact copy from original
  static constexpr int16_t unpack_16bit_sample(uint8_t b0, uint8_t b1, uint8_t b2) {
    // Reconstruct unsigned 16-bit value (left-justified in 3 bytes)
    const uint16_t unsigned_sample = ((static_cast<uint16_t>(b0) & 0x7F) << 9) |
                                     ((static_cast<uint16_t>(b1) & 0x7F) << 2) |
                                     ((static_cast<uint16_t>(b2) & 0x7F) >> 5);

    // Convert from SDS unsigned format (0x0000 = full negative) to signed
    return static_cast<int16_t>(unsigned_sample - 0x8000);
  }
};

} // namespace sds

#endif // DRUM_SYSEX_SAMPLE_PAYLOAD_HANDLER_H
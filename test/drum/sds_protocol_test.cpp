#include "test_support.h"

#include <cstddef>
#include <cstdint>

#include "etl/array.h"
#include "etl/span.h"
#include "etl/vector.h"
#include "etl/string.h"

#include "drum/sysex/sds_protocol.h"
#include "musin/hal/logger.h"

using etl::array;

// Mock time for testing
absolute_time_t mock_current_time = 0;

// Mock file operations for testing
struct TestFileOps {
  static const unsigned BlockSize = 256;

  struct Handle {
    TestFileOps &parent;
    etl::string<64> path;

    Handle(TestFileOps &parent, const etl::string_view &file_path)
        : parent(parent), path(file_path) {
      parent.file_is_open = true;
      parent.opened_file_path = file_path;
    }

    constexpr void close() {
      parent.file_is_open = false;
    }

    constexpr size_t write(const etl::span<const uint8_t> &bytes) {
      // Simulate write - copy to buffer up to capacity
      const size_t write_size = etl::min(bytes.size(),
          parent.content.size() - parent.write_position);

      for (size_t i = 0; i < write_size; ++i) {
        parent.content[parent.write_position + i] = bytes[i];
      }

      parent.write_position += write_size;
      parent.total_bytes_written += write_size;

      // Simulate write failure if requested
      return parent.should_fail_write ? 0 : write_size;
    }
  };

  constexpr etl::optional<Handle> open(const etl::string_view &path) {
    if (should_fail_open) {
      return etl::nullopt;
    }
    return Handle(*this, path);
  }

  // Test state
  bool file_is_open = false;
  bool should_fail_open = false;
  bool should_fail_write = false;
  size_t write_position = 0;
  size_t total_bytes_written = 0;
  etl::string<64> opened_file_path;
  etl::array<uint8_t, 8192> content; // Large enough for test samples

  void reset() {
    file_is_open = false;
    should_fail_open = false;
    should_fail_write = false;
    write_position = 0;
    total_bytes_written = 0;
    opened_file_path.clear();
    content.fill(0);
  }
};

// Mock logger for testing
struct TestLogger : musin::Logger {
  void log(musin::LogLevel level, etl::string_view message) override {}
  void log(musin::LogLevel level, etl::string_view message, std::int32_t value) override {}
  void log(musin::LogLevel level, etl::string_view message, std::uint32_t value) override {}
  void log(musin::LogLevel level, etl::string_view message, float value) override {}
  void set_level(musin::LogLevel level) override {}
  musin::LogLevel get_level() const override { return musin::LogLevel::DEBUG; }
};

typedef sds::Protocol<TestFileOps> SDSProtocol;

// Mock sender to capture SDS responses
struct MockSDSSender {
  etl::vector<sds::MessageType, 10> sent_types;
  etl::vector<uint8_t, 10> sent_packet_nums;

  void operator()(sds::MessageType type, uint8_t packet_num) const {
    // Note: These should be mutable for testing, but for simplicity we'll cast away const
    const_cast<MockSDSSender*>(this)->sent_types.push_back(type);
    const_cast<MockSDSSender*>(this)->sent_packet_nums.push_back(packet_num);
  }

  void clear() {
    sent_types.clear();
    sent_packet_nums.clear();
  }
};

// Helper functions for creating SDS messages
constexpr uint8_t pack_14bit_low(uint16_t value) {
  return static_cast<uint8_t>(value & 0x7F);
}

constexpr uint8_t pack_14bit_high(uint16_t value) {
  return static_cast<uint8_t>((value >> 7) & 0x7F);
}

constexpr uint8_t pack_21bit_b0(uint32_t value) {
  return static_cast<uint8_t>(value & 0x7F);
}

constexpr uint8_t pack_21bit_b1(uint32_t value) {
  return static_cast<uint8_t>((value >> 7) & 0x7F);
}

constexpr uint8_t pack_21bit_b2(uint32_t value) {
  return static_cast<uint8_t>((value >> 14) & 0x7F);
}

// Pack 16-bit signed sample into SDS 3-byte format
constexpr void pack_16bit_sample(int16_t sample, uint8_t &b0, uint8_t &b1, uint8_t &b2) {
  // Convert signed to unsigned SDS format (add 0x8000)
  const uint16_t unsigned_sample = static_cast<uint16_t>(sample + 0x8000);

  // Pack into 3 bytes (left-justified)
  b0 = static_cast<uint8_t>((unsigned_sample >> 9) & 0x7F);
  b1 = static_cast<uint8_t>((unsigned_sample >> 2) & 0x7F);
  b2 = static_cast<uint8_t>((unsigned_sample << 5) & 0x7F);
}

// Create SDS dump header for sample
etl::array<uint8_t, 17> create_sample_dump_header(
    uint16_t sample_number, uint32_t length_words, uint32_t sample_period_ns = 22675) {
  etl::array<uint8_t, 17> header;
  header[0] = sds::DUMP_HEADER;
  header[1] = pack_14bit_low(sample_number);
  header[2] = pack_14bit_high(sample_number);
  header[3] = 16; // bit depth
  header[4] = pack_21bit_b0(sample_period_ns);
  header[5] = pack_21bit_b1(sample_period_ns);
  header[6] = pack_21bit_b2(sample_period_ns);
  header[7] = pack_21bit_b0(length_words);
  header[8] = pack_21bit_b1(length_words);
  header[9] = pack_21bit_b2(length_words);
  header[10] = 0; // loop_start
  header[11] = 0;
  header[12] = 0;
  header[13] = 0; // loop_end
  header[14] = 0;
  header[15] = 0;
  header[16] = 0; // loop_type
  return header;
}

// Create SDS data packet with samples
etl::array<uint8_t, 123> create_data_packet(uint8_t packet_num,
    const etl::span<const int16_t> &samples) {
  etl::array<uint8_t, 123> packet;
  packet[0] = sds::DATA_PACKET;
  packet[1] = packet_num;

  // Pack samples into data payload (max 40 samples)
  const size_t num_samples = etl::min(samples.size(), static_cast<size_t>(40));
  for (size_t i = 0; i < num_samples; ++i) {
    const size_t offset = 2 + i * 3;
    pack_16bit_sample(samples[i], packet[offset], packet[offset + 1], packet[offset + 2]);
  }

  // Fill remaining data with zeros
  for (size_t i = num_samples * 3; i < 120; ++i) {
    packet[2 + i] = 0;
  }

  // Calculate checksum
  uint8_t checksum = 0x7E ^ 0x65 ^ sds::DATA_PACKET ^ packet_num;
  for (size_t i = 2; i < 122; ++i) {
    checksum ^= packet[i];
  }
  packet[122] = checksum & 0x7F;

  return packet;
}

// Verify unpacked PCM data matches expected samples
bool verify_pcm_data(const TestFileOps &file_ops, const etl::span<const int16_t> &expected_samples) {
  if (file_ops.total_bytes_written != expected_samples.size() * 2) {
    return false;
  }

  for (size_t i = 0; i < expected_samples.size(); ++i) {
    const size_t byte_offset = i * 2;
    if (byte_offset + 1 >= file_ops.content.size()) {
      return false;
    }

    // Read little-endian 16-bit
    const int16_t unpacked_sample = static_cast<int16_t>(
        file_ops.content[byte_offset] | (file_ops.content[byte_offset + 1] << 8));

    if (unpacked_sample != expected_samples[i]) {
      return false;
    }
  }

  return true;
}

TEST_CASE("SDS Protocol - Basic sample transfer") {
  CONST_BODY(({
    TestFileOps file_ops;
    TestLogger logger;
    SDSProtocol protocol(file_ops, logger);
    MockSDSSender sender;

    // Create test samples
    etl::array<int16_t, 5> test_samples = {-32768, -1, 0, 1, 32767};

    // Send dump header for 5 samples (sample number 1)
    auto header = create_sample_dump_header(1, 5);
    auto result = protocol.process_message(etl::span<const uint8_t>{header.data(), header.size()},
                                          sender, get_absolute_time());

    REQUIRE(result == sds::Result::OK);
    REQUIRE(protocol.get_state() == sds::State::ReceivingData);
    REQUIRE(file_ops.file_is_open == true);
    REQUIRE(file_ops.opened_file_path == "/01.pcm");
    REQUIRE(sender.sent_types.size() == 1);
    REQUIRE(sender.sent_types[0] == sds::ACK);

    sender.clear();

    // Send data packet with 5 samples
    auto data_packet = create_data_packet(0, test_samples);
    result = protocol.process_message(etl::span<const uint8_t>{data_packet.data(), data_packet.size()},
                                     sender, get_absolute_time());

    REQUIRE(result == sds::Result::SampleComplete);
    REQUIRE(protocol.get_state() == sds::State::Idle);
    REQUIRE(file_ops.file_is_open == false);
    REQUIRE(sender.sent_types.size() == 1);
    REQUIRE(sender.sent_types[0] == sds::ACK);

    // Verify unpacked PCM data
    REQUIRE(verify_pcm_data(file_ops, test_samples));
  }));
}

TEST_CASE("SDS Protocol - Single sample transfer") {
  CONST_BODY(({
    TestFileOps file_ops;
    TestLogger logger;
    SDSProtocol protocol(file_ops, logger);
    MockSDSSender sender;

    // Single sample
    etl::array<int16_t, 1> test_samples = {12345};

    auto header = create_sample_dump_header(0, 1);
    auto result = protocol.process_message(etl::span<const uint8_t>{header.data(), header.size()},
                                          sender, get_absolute_time());
    REQUIRE(result == sds::Result::OK);

    sender.clear();

    auto data_packet = create_data_packet(0, test_samples);
    result = protocol.process_message(etl::span<const uint8_t>{data_packet.data(), data_packet.size()},
                                     sender, get_absolute_time());

    REQUIRE(result == sds::Result::SampleComplete);
    REQUIRE(verify_pcm_data(file_ops, test_samples));
  }));
}

TEST_CASE("SDS Protocol - Exactly 40 samples (one full packet)") {
  CONST_BODY(({
    TestFileOps file_ops;
    TestLogger logger;
    SDSProtocol protocol(file_ops, logger);
    MockSDSSender sender;

    // 40 samples exactly
    etl::array<int16_t, 40> test_samples;
    for (size_t i = 0; i < 40; ++i) {
      test_samples[i] = static_cast<int16_t>(i - 20); // Range -20 to 19
    }

    auto header = create_sample_dump_header(5, 40);
    auto result = protocol.process_message(etl::span<const uint8_t>{header.data(), header.size()},
                                          sender, get_absolute_time());
    REQUIRE(result == sds::Result::OK);

    sender.clear();

    auto data_packet = create_data_packet(0, test_samples);
    result = protocol.process_message(etl::span<const uint8_t>{data_packet.data(), data_packet.size()},
                                     sender, get_absolute_time());

    REQUIRE(result == sds::Result::SampleComplete);
    REQUIRE(verify_pcm_data(file_ops, test_samples));
  }));
}

TEST_CASE("SDS Protocol - 41 samples (spans two packets)") {
  CONST_BODY(({
    TestFileOps file_ops;
    TestLogger logger;
    SDSProtocol protocol(file_ops, logger);
    MockSDSSender sender;

    etl::array<int16_t, 41> test_samples;
    for (size_t i = 0; i < 41; ++i) {
      test_samples[i] = static_cast<int16_t>(i * 100);
    }

    auto header = create_sample_dump_header(2, 41);
    auto result = protocol.process_message(etl::span<const uint8_t>{header.data(), header.size()},
                                          sender, get_absolute_time());
    REQUIRE(result == sds::Result::OK);
    REQUIRE(file_ops.opened_file_path == "/02.pcm");

    sender.clear();

    // First packet - 40 samples
    auto first_packet = create_data_packet(0, etl::span<const int16_t>{test_samples.data(), 40});
    result = protocol.process_message(etl::span<const uint8_t>{first_packet.data(), first_packet.size()},
                                     sender, get_absolute_time());

    REQUIRE(result == sds::Result::OK);
    REQUIRE(protocol.get_state() == sds::State::ReceivingData);
    REQUIRE(sender.sent_types.size() == 1);
    REQUIRE(sender.sent_types[0] == sds::ACK);

    sender.clear();

    // Second packet - 1 sample
    auto second_packet = create_data_packet(1, etl::span<const int16_t>{test_samples.data() + 40, 1});
    result = protocol.process_message(etl::span<const uint8_t>{second_packet.data(), second_packet.size()},
                                     sender, get_absolute_time());

    REQUIRE(result == sds::Result::SampleComplete);
    REQUIRE(protocol.get_state() == sds::State::Idle);
    REQUIRE(verify_pcm_data(file_ops, test_samples));
  }));
}

TEST_CASE("SDS Protocol - Checksum error handling") {
  CONST_BODY(({
    TestFileOps file_ops;
    TestLogger logger;
    SDSProtocol protocol(file_ops, logger);
    MockSDSSender sender;

    etl::array<int16_t, 1> test_samples = {100};

    auto header = create_sample_dump_header(3, 1);
    auto result = protocol.process_message(etl::span<const uint8_t>{header.data(), header.size()},
                                          sender, get_absolute_time());
    REQUIRE(result == sds::Result::OK);

    sender.clear();

    // Create packet with invalid checksum
    auto data_packet = create_data_packet(0, test_samples);
    data_packet[122] = 0x00; // Wrong checksum

    result = protocol.process_message(etl::span<const uint8_t>{data_packet.data(), data_packet.size()},
                                     sender, get_absolute_time());

    REQUIRE(result == sds::Result::ChecksumError);
    REQUIRE(sender.sent_types.size() == 1);
    REQUIRE(sender.sent_types[0] == sds::NAK);
    REQUIRE(sender.sent_packet_nums[0] == 0);
    REQUIRE(file_ops.total_bytes_written == 0); // No data should be written
  }));
}

TEST_CASE("SDS Protocol - File write error handling") {
  CONST_BODY(({
    TestFileOps file_ops;
    TestLogger logger;
    SDSProtocol protocol(file_ops, logger);
    MockSDSSender sender;

    etl::array<int16_t, 1> test_samples = {100};

    auto header = create_sample_dump_header(4, 1);
    auto result = protocol.process_message(etl::span<const uint8_t>{header.data(), header.size()},
                                          sender, get_absolute_time());
    REQUIRE(result == sds::Result::OK);

    sender.clear();

    // Make file write fail
    file_ops.should_fail_write = true;

    auto data_packet = create_data_packet(0, test_samples);
    result = protocol.process_message(etl::span<const uint8_t>{data_packet.data(), data_packet.size()},
                                     sender, get_absolute_time());

    REQUIRE(result == sds::Result::FileError);
    REQUIRE(protocol.get_state() == sds::State::Idle);
    REQUIRE(file_ops.file_is_open == false);
    REQUIRE(sender.sent_types.size() == 1);
    REQUIRE(sender.sent_types[0] == sds::NAK);
  }));
}

TEST_CASE("SDS Protocol - Cancel message handling") {
  CONST_BODY(({
    TestFileOps file_ops;
    TestLogger logger;
    SDSProtocol protocol(file_ops, logger);
    MockSDSSender sender;

    // Start a transfer
    auto header = create_sample_dump_header(7, 100);
    auto result = protocol.process_message(etl::span<const uint8_t>{header.data(), header.size()},
                                          sender, get_absolute_time());
    REQUIRE(result == sds::Result::OK);
    REQUIRE(protocol.get_state() == sds::State::ReceivingData);

    sender.clear();

    // Send cancel message
    etl::array<uint8_t, 1> cancel_msg = {sds::CANCEL};
    result = protocol.process_message(etl::span<const uint8_t>{cancel_msg.data(), cancel_msg.size()},
                                     sender, get_absolute_time());

    REQUIRE(result == sds::Result::Cancelled);
    REQUIRE(protocol.get_state() == sds::State::Idle);
    REQUIRE(file_ops.file_is_open == false);
    REQUIRE(sender.sent_types.empty()); // No reply for cancel
  }));
}

TEST_CASE("SDS Protocol - Invalid bit depth rejection") {
  CONST_BODY(({
    TestFileOps file_ops;
    TestLogger logger;
    SDSProtocol protocol(file_ops, logger);
    MockSDSSender sender;

    // Create header with 8-bit depth
    auto header = create_sample_dump_header(8, 10);
    header[3] = 8; // Invalid bit depth

    auto result = protocol.process_message(etl::span<const uint8_t>{header.data(), header.size()},
                                          sender, get_absolute_time());

    REQUIRE(result == sds::Result::InvalidMessage);
    REQUIRE(protocol.get_state() == sds::State::Idle);
    REQUIRE(file_ops.file_is_open == false);
    REQUIRE(sender.sent_types.size() == 1);
    REQUIRE(sender.sent_types[0] == sds::NAK);
  }));
}

TEST_CASE("SDS Protocol - Out of order packet handling") {
  CONST_BODY(({
    TestFileOps file_ops;
    TestLogger logger;
    SDSProtocol protocol(file_ops, logger);
    MockSDSSender sender;

    etl::array<int16_t, 2> test_samples = {1000, 2000};

    auto header = create_sample_dump_header(9, 2);
    auto result = protocol.process_message(etl::span<const uint8_t>{header.data(), header.size()},
                                          sender, get_absolute_time());
    REQUIRE(result == sds::Result::OK);

    sender.clear();

    // Send packet with wrong number (5 instead of 0)
    auto data_packet = create_data_packet(5, test_samples);
    result = protocol.process_message(etl::span<const uint8_t>{data_packet.data(), data_packet.size()},
                                     sender, get_absolute_time());

    // Should accept out-of-order packets (current implementation)
    REQUIRE(result == sds::Result::SampleComplete);
    REQUIRE(verify_pcm_data(file_ops, test_samples));
  }));
}

TEST_CASE("SDS Protocol - Filename generation") {
  CONST_BODY(({
    TestFileOps file_ops;
    TestLogger logger;
    SDSProtocol protocol(file_ops, logger);
    MockSDSSender sender;

    // Test various sample numbers
    const uint16_t sample_numbers[] = {0, 1, 9, 10, 99, 127};
    const char* expected_filenames[] = {"/00.pcm", "/01.pcm", "/09.pcm", "/10.pcm", "/99.pcm", "/127.pcm"};

    for (size_t i = 0; i < 6; ++i) {
      file_ops.reset();

      auto header = create_sample_dump_header(sample_numbers[i], 1);
      auto result = protocol.process_message(etl::span<const uint8_t>{header.data(), header.size()},
                                            sender, get_absolute_time());

      REQUIRE(result == sds::Result::OK);
      REQUIRE(file_ops.opened_file_path == expected_filenames[i]);
    }
  }));
}

TEST_CASE("SDS Protocol - Large sample transfer (80 samples)") {
  CONST_BODY(({
    TestFileOps file_ops;
    TestLogger logger;
    SDSProtocol protocol(file_ops, logger);
    MockSDSSender sender;

    etl::array<int16_t, 80> test_samples;
    for (size_t i = 0; i < 80; ++i) {
      test_samples[i] = static_cast<int16_t>(i * 400 - 16000);
    }

    auto header = create_sample_dump_header(10, 80);
    auto result = protocol.process_message(etl::span<const uint8_t>{header.data(), header.size()},
                                          sender, get_absolute_time());
    REQUIRE(result == sds::Result::OK);

    sender.clear();

    // First packet - samples 0-39
    auto first_packet = create_data_packet(0, etl::span<const int16_t>{test_samples.data(), 40});
    result = protocol.process_message(etl::span<const uint8_t>{first_packet.data(), first_packet.size()},
                                     sender, get_absolute_time());
    REQUIRE(result == sds::Result::OK);

    sender.clear();

    // Second packet - samples 40-79
    auto second_packet = create_data_packet(1, etl::span<const int16_t>{test_samples.data() + 40, 40});
    result = protocol.process_message(etl::span<const uint8_t>{second_packet.data(), second_packet.size()},
                                     sender, get_absolute_time());

    REQUIRE(result == sds::Result::SampleComplete);
    REQUIRE(verify_pcm_data(file_ops, test_samples));
  }));
}
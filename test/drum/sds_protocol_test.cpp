#include <catch2/catch_test_macros.hpp>

#include <cstdint>

#include "etl/array.h"
#include "etl/span.h"
#include "etl/vector.h"

#include "musin/hal/null_logger.h"

#include "drum/sysex/sds_protocol.h"

absolute_time_t mock_current_time = 0;

namespace {

// Minimal file mock: every open succeeds and writes are accepted.
struct TestFileOps {
  struct Handle {
    TestFileOps &parent;

    constexpr Handle(TestFileOps &parent) : parent(parent) {
      parent.open_count++;
      parent.file_is_open = true;
    }

    constexpr void close() {
      parent.file_is_open = false;
    }

    constexpr size_t write(const etl::span<const uint8_t> &bytes) {
      parent.bytes_written += bytes.size();
      return bytes.size();
    }
  };

  constexpr Handle open(const etl::string_view &) {
    return Handle(*this);
  }

  bool file_is_open = false;
  unsigned open_count = 0;
  size_t bytes_written = 0;
};

using Protocol = sds::Protocol<TestFileOps>;

// Records replies so tests can assert no traffic is generated on timeout.
struct MockSender {
  etl::vector<sds::MessageType, 16> replies;
  void operator()(sds::MessageType type, uint8_t) {
    replies.push_back(type);
  }
};

constexpr uint64_t ONE_SECOND_US = 1000000;

// Builds an SDS dump-header payload (as seen by process_message) for a
// 16-bit sample of the given word length.
etl::array<uint8_t, 17> make_dump_header(uint32_t length_words) {
  return {sds::DUMP_HEADER,
          0x1E, // sample number low (= 30)
          0x00, // sample number high
          16,   // bit depth
          0x10, 0x30, 0x01, // sample period (~44.1k), arbitrary nonzero
          static_cast<uint8_t>(length_words & 0x7F),
          static_cast<uint8_t>((length_words >> 7) & 0x7F),
          static_cast<uint8_t>((length_words >> 14) & 0x7F),
          0x00, 0x00, 0x00, // loop start
          0x00, 0x00, 0x00, // loop end
          0x00};            // loop type
}

// Builds an all-zero data packet with a valid checksum.
etl::array<uint8_t, 123> make_data_packet(uint8_t packet_num) {
  etl::array<uint8_t, 123> packet{};
  packet[0] = sds::DATA_PACKET;
  packet[1] = packet_num;
  // Data bytes [2..121] left zero. Checksum over all-zero data:
  uint8_t checksum = 0x7E ^ 0x65 ^ sds::DATA_PACKET ^ packet_num;
  packet[122] = checksum & 0x7F;
  return packet;
}

TEST_CASE("SDS transfer times out after 30s of inactivity") {
  TestFileOps file_ops;
  musin::NullLogger logger;
  Protocol protocol(file_ops, logger);
  MockSender sender;

  const auto header = make_dump_header(400); // 800 bytes, multi-packet
  const absolute_time_t start = ONE_SECOND_US;
  protocol.process_message(etl::span<const uint8_t>{header}, sender, start);

  REQUIRE(protocol.is_busy());
  REQUIRE(file_ops.file_is_open);

  SECTION("just before the deadline the transfer survives") {
    const absolute_time_t now = start + Protocol::TIMEOUT_US;
    REQUIRE_FALSE(protocol.check_timeout(now));
    REQUIRE(protocol.is_busy());
  }

  SECTION("past the deadline the transfer is aborted silently") {
    const absolute_time_t now = start + Protocol::TIMEOUT_US + 1;
    sender.replies.clear();
    REQUIRE(protocol.check_timeout(now));
    REQUIRE_FALSE(protocol.is_busy());
    REQUIRE_FALSE(file_ops.file_is_open);
    REQUIRE(sender.replies.empty()); // host is gone; no reply
  }
}

TEST_CASE("SDS activity resets the inactivity timer") {
  TestFileOps file_ops;
  musin::NullLogger logger;
  Protocol protocol(file_ops, logger);
  MockSender sender;

  const auto header = make_dump_header(400);
  const absolute_time_t start = ONE_SECOND_US;
  protocol.process_message(etl::span<const uint8_t>{header}, sender, start);

  // A data packet 20s in keeps the transfer alive and resets the clock.
  const absolute_time_t packet_time = start + 20 * ONE_SECOND_US;
  REQUIRE_FALSE(protocol.check_timeout(packet_time));
  const auto packet = make_data_packet(0);
  protocol.process_message(etl::span<const uint8_t>{packet}, sender, packet_time);
  REQUIRE(protocol.is_busy());

  // 25s after the packet (45s after the header) is still within the window.
  REQUIRE_FALSE(protocol.check_timeout(packet_time + 25 * ONE_SECOND_US));
  REQUIRE(protocol.is_busy());

  // 31s after the packet finally trips the timeout.
  REQUIRE(protocol.check_timeout(packet_time + 31 * ONE_SECOND_US));
  REQUIRE_FALSE(protocol.is_busy());
}

TEST_CASE("SDS check_timeout is a no-op while idle") {
  TestFileOps file_ops;
  musin::NullLogger logger;
  Protocol protocol(file_ops, logger);

  REQUIRE_FALSE(protocol.is_busy());
  REQUIRE_FALSE(protocol.check_timeout(1000 * ONE_SECOND_US));
  REQUIRE_FALSE(protocol.is_busy());
}

} // namespace

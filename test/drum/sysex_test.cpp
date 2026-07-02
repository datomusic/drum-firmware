#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>

#include "etl/array.h"
#include "etl/optional.h"
#include "etl/span.h"
#include "etl/string.h"
#include "etl/string_view.h"
#include "etl/vector.h"

#include "drum/config.h"
#include "drum/sysex/protocol.h"
#include "musin/hal/null_logger.h"

namespace {

struct TestFileOps {
  static constexpr unsigned BlockSize = 256;

  struct Handle {
    TestFileOps &parent;

    explicit Handle(TestFileOps &parent) : parent(parent) {
      parent.file_is_open = true;
    }

    void close() {
      parent.file_is_open = false;
    }

    size_t write(const etl::span<const uint8_t> &bytes) {
      for (uint8_t byte : bytes) {
        if (parent.byte_count < parent.content.size()) {
          parent.content[parent.byte_count] = byte;
        }
        ++parent.byte_count;
      }
      return bytes.size();
    }
  };

  etl::optional<Handle> open(const etl::string_view &path) {
    last_path.assign(path.begin(), path.end());
    return Handle(*this);
  }

  bool format() {
    return true;
  }

  bool file_is_open = false;
  size_t byte_count = 0;
  etl::array<uint8_t, BlockSize> content{};
  etl::string<drum::config::MAX_PATH_LENGTH> last_path;
};

using Protocol = sysex::Protocol<TestFileOps>;
using State = Protocol::State;

// handle_chunk takes the sender by value, so record into a vector the
// sender references rather than a member.
struct MockSender {
  etl::vector<Protocol::Tag, 10> &sent_tags;
  void operator()(Protocol::Tag tag) {
    sent_tags.push_back(tag);
  }
};

// Chunk header as seen by Protocol (0xF0 already stripped):
// manufacturer ID (3 bytes) followed by device ID.
constexpr uint8_t MFR0 = drum::config::sysex::MANUFACTURER_ID_0;
constexpr uint8_t MFR1 = drum::config::sysex::MANUFACTURER_ID_1;
constexpr uint8_t MFR2 = drum::config::sysex::MANUFACTURER_ID_2;
constexpr uint8_t DEV = drum::config::sysex::DEVICE_ID;

} // namespace

TEST_CASE("Protocol with empty bytes") {
  TestFileOps file_ops;
  musin::NullLogger logger;
  Protocol protocol(file_ops, logger);
  etl::vector<Protocol::Tag, 10> sent_tags;
  MockSender sender{sent_tags};

  const sysex::Chunk chunk(nullptr, 0);
  const auto result = protocol.handle_chunk(chunk, sender, absolute_time_t{});

  REQUIRE(result == Protocol::Result::ShortMessage);
  REQUIRE(protocol.get_state() == State::Idle);
  REQUIRE(sent_tags.empty());
}

TEST_CASE("Protocol rejects wrong manufacturer ID") {
  TestFileOps file_ops;
  musin::NullLogger logger;
  Protocol protocol(file_ops, logger);
  etl::vector<Protocol::Tag, 10> sent_tags;
  MockSender sender{sent_tags};

  const uint8_t message[] = {0x7D, 0x01, 0x02, DEV,
                             Protocol::RequestFirmwareVersion};
  const auto result = protocol.handle_chunk(
      sysex::Chunk(message, sizeof(message)), sender, absolute_time_t{});

  REQUIRE(result == Protocol::Result::InvalidManufacturer);
  REQUIRE(sent_tags.empty());
}

TEST_CASE("Protocol receives file data") {
  TestFileOps file_ops;
  musin::NullLogger logger;
  REQUIRE(file_ops.file_is_open == false);
  Protocol protocol(file_ops, logger);
  etl::vector<Protocol::Tag, 10> sent_tags;
  MockSender sender{sent_tags};
  const absolute_time_t now{};

  // BeginFileWrite with a one-character file name '@' (64), encoded with
  // the 3-bytes-to-16-bit body codec: 64 -> {0, 0, 64}.
  const uint8_t begin_file_write[] = {
      MFR0, MFR1, MFR2, DEV, Protocol::BeginFileWrite, 0, 0, 64};
  auto result = protocol.handle_chunk(
      sysex::Chunk(begin_file_write, sizeof(begin_file_write)), sender, now);

  REQUIRE(result == Protocol::Result::OK);
  REQUIRE(protocol.get_state() == State::FileTransfer);
  REQUIRE(file_ops.file_is_open == true);
  REQUIRE(file_ops.last_path == "/@");
  REQUIRE(file_ops.byte_count == 0);
  REQUIRE(sent_tags.size() == 1);
  REQUIRE(sent_tags[0] == Protocol::Tag::Ack);

  sent_tags.clear();

  // FileBytes carries a raw 7-to-8 encoded payload: 7 data bytes followed by
  // an MSB byte. All MSBs zero here, so the decoded bytes are 1..7.
  const uint8_t byte_transfer[] = {MFR0, MFR1, MFR2, DEV, Protocol::FileBytes,
                                   1,    2,    3,    4,   5,
                                   6,    7,    0};
  result = protocol.handle_chunk(
      sysex::Chunk(byte_transfer, sizeof(byte_transfer)), sender, now);

  REQUIRE(result == Protocol::Result::OK);
  // Bytes are buffered until the block fills or the transfer ends.
  REQUIRE(file_ops.byte_count == 0);
  REQUIRE(sent_tags.size() == 1);
  REQUIRE(sent_tags[0] == Protocol::Tag::Ack);

  sent_tags.clear();

  const uint8_t end_write[] = {MFR0, MFR1, MFR2, DEV,
                               Protocol::EndFileTransfer};
  result = protocol.handle_chunk(sysex::Chunk(end_write, sizeof(end_write)),
                                 sender, now);

  REQUIRE(result == Protocol::Result::FileWritten);
  REQUIRE(protocol.get_state() == State::Idle);
  REQUIRE(file_ops.file_is_open == false);
  REQUIRE(file_ops.byte_count == 7);
  for (size_t i = 0; i < 7; ++i) {
    REQUIRE(file_ops.content[i] == i + 1);
  }
  REQUIRE(sent_tags.size() == 1);
  REQUIRE(sent_tags[0] == Protocol::Tag::Ack);
}

TEST_CASE("Protocol decodes MSBs in FileBytes payload") {
  TestFileOps file_ops;
  musin::NullLogger logger;
  Protocol protocol(file_ops, logger);
  etl::vector<Protocol::Tag, 10> sent_tags;
  MockSender sender{sent_tags};
  const absolute_time_t now{};

  const uint8_t begin_file_write[] = {
      MFR0, MFR1, MFR2, DEV, Protocol::BeginFileWrite, 0, 0, 64};
  protocol.handle_chunk(
      sysex::Chunk(begin_file_write, sizeof(begin_file_write)), sender, now);

  // MSB byte 0b0000001 sets bit 7 of the first data byte: 0x7F -> 0xFF.
  const uint8_t byte_transfer[] = {MFR0, MFR1, MFR2, DEV, Protocol::FileBytes,
                                   0x7F, 0,    0,    0,   0,
                                   0,    0,    0x01};
  protocol.handle_chunk(sysex::Chunk(byte_transfer, sizeof(byte_transfer)),
                        sender, now);

  const uint8_t end_write[] = {MFR0, MFR1, MFR2, DEV,
                               Protocol::EndFileTransfer};
  protocol.handle_chunk(sysex::Chunk(end_write, sizeof(end_write)), sender,
                        now);

  REQUIRE(file_ops.byte_count == 7);
  REQUIRE(file_ops.content[0] == 0xFF);
  REQUIRE(file_ops.content[1] == 0);
}

TEST_CASE("Protocol maps no-body commands to results") {
  TestFileOps file_ops;
  musin::NullLogger logger;
  Protocol protocol(file_ops, logger);
  etl::vector<Protocol::Tag, 10> sent_tags;
  MockSender sender{sent_tags};

  const uint8_t request_version[] = {MFR0, MFR1, MFR2, DEV,
                                     Protocol::RequestFirmwareVersion};
  const auto result =
      protocol.handle_chunk(sysex::Chunk(request_version,
                                         sizeof(request_version)),
                            sender, absolute_time_t{});

  REQUIRE(result == Protocol::Result::PrintFirmwareVersion);
}

// ---------------------------------------------------------------------------
// SDS Protocol tests (for issue #550: sample slot tracking)
// ---------------------------------------------------------------------------
//
// SysExHandler::update() reads sds_protocol_.current_sample_number_opt() to
// determine the sample slot for display events.  When a transfer completes
// (Result::SampleComplete), the protocol resets to Idle and
// current_sample_number_opt() returns nullopt.
//
// sysex_handler.cpp captures `last_notified_sample_slot_` *before* the
// protocol goes Idle, so the "transfer finished" event can carry the last
// known slot instead of nullopt.  These tests verify the protocol's slot
// reporting behavior that makes that capture necessary.

#include "drum/sysex/sds_protocol.h"

namespace {

struct SdsTestFileOps {
  static const unsigned BlockSize = 4096;

  struct Handle {
    SdsTestFileOps &parent;
    constexpr Handle(SdsTestFileOps &parent) : parent(parent) {
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

  constexpr etl::optional<Handle> open(const etl::string_view &) {
    return Handle(*this);
  }

  bool file_is_open = false;
  size_t bytes_written = 0;
};

// Build a minimal SDS Dump Header for sample_number, bit_depth=16,
// one 16-bit word long (so a single data packet completes the transfer).
etl::array<uint8_t, 19> make_dump_header(uint16_t sample_number) {
  // SDS DUMP_HEADER layout (after 0x7E channel byte stripped by SysExHandler):
  // [0] msg type 0x01, [1] sample_num_lsb, [2] sample_num_msb,
  // [3] bit_depth, [4-6] period_ns (21-bit), [7-9] length_words (21-bit),
  // [10-12] loop_start, [13-15] loop_end, [16] loop_type, [17] checksum
  etl::array<uint8_t, 19> h{};
  h[0] = sds::DUMP_HEADER;
  h[1] = sample_number & 0x7F;        // LSB
  h[2] = (sample_number >> 7) & 0x7F; // MSB
  h[3] = 16;                          // bit depth
  // period: 22676 ns ≈ 44100 Hz => 22676 = 0x587C
  h[4] = 0x7C & 0x7F;
  h[5] = (0x587C >> 7) & 0x7F;
  h[6] = (0x587C >> 14) & 0x7F;
  // length_words = 1 (smallest valid transfer)
  h[7] = 1;
  h[8] = 0;
  h[9] = 0;
  // loop points & type: 0
  h[10] = h[11] = h[12] = h[13] = h[14] = h[15] = h[16] = 0;
  // No checksum needed for header in sds_protocol.h (it's not validated)
  return h;
}

// Build a valid SDS Data Packet for a single sample word (2 bytes = 1 word).
// The protocol expects exactly 123 bytes: [type][packet_num][120
// data][checksum]
etl::array<uint8_t, 123> make_data_packet(uint8_t packet_num) {
  etl::array<uint8_t, 123> pkt{};
  pkt[0] = sds::DATA_PACKET;
  pkt[1] = packet_num;
  // data bytes [2..121] all zero (silence)
  // checksum = XOR of (0x7E ^ 0x65 ^ 0x02 ^ packet_num ^ all_data)
  uint8_t cs = 0x7E ^ 0x65 ^ sds::DATA_PACKET ^ packet_num;
  for (size_t i = 2; i < 122; ++i)
    cs ^= pkt[i];
  pkt[122] = cs & 0x7F;
  return pkt;
}

} // namespace

TEST_CASE(
    "SDS: sample slot is reported during transfer and cleared on complete",
    "[sds][issue-550]") {
  SdsTestFileOps file_ops;
  musin::NullLogger logger;

  sds::Protocol<SdsTestFileOps> protocol(file_ops, logger);

  auto sender = [](sds::MessageType, uint8_t) {};
  absolute_time_t t{};

  // Initially no slot
  REQUIRE_FALSE(protocol.current_sample_number_opt().has_value());
  REQUIRE_FALSE(protocol.is_busy());

  // Send Dump Header for sample 7
  auto header = make_dump_header(7);
  protocol.process_message(
      etl::span<const uint8_t>{header.data(), header.size()}, sender, t);

  REQUIRE(protocol.is_busy());
  REQUIRE(protocol.current_sample_number_opt().has_value());
  REQUIRE(protocol.current_sample_number_opt().value() == 7);

  // Send Data Packet to complete the 1-word transfer
  auto pkt = make_data_packet(0);
  auto result = protocol.process_message(
      etl::span<const uint8_t>{pkt.data(), pkt.size()}, sender, t);

  REQUIRE(result == sds::Result::SampleComplete);
  // Protocol is now Idle; slot is cleared
  REQUIRE_FALSE(protocol.is_busy());
  REQUIRE_FALSE(protocol.current_sample_number_opt().has_value());
  // SysExHandler captures last_notified_sample_slot_ *before* this point,
  // which is what its slot-tracking fix relies on.
}

TEST_CASE("SDS: slot nullopt when never started", "[sds][issue-550]") {
  SdsTestFileOps file_ops;
  musin::NullLogger logger;
  sds::Protocol<SdsTestFileOps> protocol(file_ops, logger);

  REQUIRE_FALSE(protocol.current_sample_number_opt().has_value());
}

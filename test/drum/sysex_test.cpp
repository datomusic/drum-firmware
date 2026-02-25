#include "test_support.h"

#include <cstddef>
#include <cstdint>

#include "etl/array.h"
#include "etl/span.h"
#include "etl/vector.h"

#include "drum/sysex/protocol.h"

using etl::array;

struct TestFileOps {
  static const unsigned BlockSize = 256;

  struct Handle {
    TestFileOps &parent;

    constexpr Handle(TestFileOps &parent) : parent(parent) {
      parent.file_is_open = true;
    }

    constexpr void close() {
      parent.file_is_open = false;
    }

    constexpr size_t write(const etl::span<const uint8_t> &bytes) {
      // TODO: Indicate error when write is truncated.
      etl::copy_n(bytes.cbegin(), std::min(bytes.size(), parent.content.size()),
                  parent.content.begin());
      parent.byte_count = bytes.size();
      return bytes.size();
    }
  };

  // Handle should close upon destruction
  constexpr Handle open(const etl::string_view &path) {
    return Handle(*this);
  }

  bool file_is_open = false;
  size_t byte_count = 0;
  etl::array<uint8_t, BlockSize> content;
};

typedef sysex::Protocol<TestFileOps> Protocol;
typedef Protocol::State State;

struct MockSender {
  etl::vector<Protocol::Tag, 10> sent_tags;
  void operator()(Protocol::Tag tag) {
    sent_tags.push_back(tag);
  }
};

TEST_CASE("Protocol with empty bytes") {
  CONST_BODY(({
    TestFileOps file_ops;
    Protocol protocol(file_ops);
    MockSender sender;
    const uint8_t data[0] = {};
    sysex::Chunk chunk(data, 0);
    protocol.handle_chunk(chunk, sender);
    REQUIRE(protocol.__get_state() == State::Idle);
    REQUIRE(sender.sent_tags.empty());
  }));
}

TEST_CASE("Protocol receives file data") {
  CONST_BODY(({
    TestFileOps file_ops;
    REQUIRE(file_ops.file_is_open == false);
    Protocol protocol(file_ops);
    MockSender sender;

    // TODO: Simplify building of SysEx messages, so we don't have to repeat the
    // header in each one

    const uint8_t begin_file_write[] = {
        0, 0x7D, 0x65, 0, 0, Protocol::BeginFileWrite,
        0, 0,    64}; // Represents a file-name with ASCII character '@'
    protocol.handle_chunk(
        sysex::Chunk(begin_file_write, sizeof(begin_file_write)), sender);

    REQUIRE(protocol.__get_state() == State::FileTransfer);
    REQUIRE(file_ops.file_is_open == true);
    REQUIRE(file_ops.byte_count == 0);
    REQUIRE(sender.sent_tags.size() == 1);
    REQUIRE(sender.sent_tags[0] == Protocol::Tag::Ack);

    sender.sent_tags.clear();

    const uint8_t byte_transfer[] = {0, 0x7D, 0x65, 0, 0, Protocol::FileBytes,
                                     0, 0,    127};
    protocol.handle_chunk(sysex::Chunk(byte_transfer, sizeof(byte_transfer)),
                          sender);
    REQUIRE(file_ops.byte_count == 2);
    REQUIRE(file_ops.content[0] == 127);
    REQUIRE(file_ops.content[1] == 0);
    REQUIRE(sender.sent_tags.size() == 1);
    REQUIRE(sender.sent_tags[0] == Protocol::Tag::Ack);

    sender.sent_tags.clear();

    const uint8_t end_write[] = {0, 0x7D, 0x65,
                                 0, 0,    Protocol::EndFileTransfer};
    protocol.handle_chunk(sysex::Chunk(end_write, sizeof(end_write)), sender);
    REQUIRE(protocol.__get_state() == State::Idle);
    REQUIRE(file_ops.file_is_open == false);
    REQUIRE(sender.sent_tags.size() == 1);
    REQUIRE(sender.sent_tags[0] == Protocol::Tag::Ack);
  }));
}

static constexpr uint8_t syx_pack1(uint16_t value) {
  return (uint8_t)((value >> 14) & 0x7F);
}

static constexpr uint8_t syx_pack2(uint16_t value) {
  return (uint8_t)((value >> 7) & 0x7F);
}

static constexpr uint8_t syx_pack3(uint16_t value) {
  return (uint8_t)(value & 0x7F);
}

TEST_CASE("decoder decodes a byte") {
  CONST_BODY(({
    const auto v1 = 100;
    const auto v2 = 0;
    const auto v3 = 127;

    const uint8_t sysex[9] = {
        syx_pack1(v1), syx_pack2(v1), syx_pack3(v1), v2, v2, v2,
        syx_pack1(v3), syx_pack2(v3), syx_pack3(v3)};
    array<uint16_t, 9> bytes;
    const auto byte_count = sysex::codec::decode<9>(sysex, sysex + 9, bytes);

    REQUIRE(byte_count == 3);
    REQUIRE(bytes[0] == 100);
    REQUIRE(bytes[1] == 0);
    REQUIRE(bytes[2] == 127);
  }));
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
// Our fix in sysex_handler.cpp captures `last_notified_sample_slot_` *before*
// the protocol goes Idle, so the "transfer finished" event can carry the last
// known slot instead of nullopt.  These tests verify the protocol's slot
// reporting behavior that makes that capture necessary.

#include "drum/sysex/sds_protocol.h"
#include "musin/hal/null_logger.h"

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
static etl::array<uint8_t, 19> make_dump_header(uint16_t sample_number) {
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
static etl::array<uint8_t, 123> make_data_packet(uint8_t packet_num) {
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
  // which is what our fix relies on.
}

TEST_CASE("SDS: slot nullopt when never started", "[sds][issue-550]") {
  SdsTestFileOps file_ops;
  musin::NullLogger logger;
  sds::Protocol<SdsTestFileOps> protocol(file_ops, logger);

  REQUIRE_FALSE(protocol.current_sample_number_opt().has_value());
}

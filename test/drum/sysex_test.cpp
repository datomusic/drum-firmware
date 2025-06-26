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

    // TODO: Simplify building of SysEx messages, so we don't have to repeat the header in each one

    const uint8_t begin_file_write[] = {
        0, 0x7D, 0x65, 0, 0, Protocol::BeginFileWrite,
        0, 0,    64}; // Represents a file-name with ASCII character '@'
    protocol.handle_chunk(sysex::Chunk(begin_file_write, sizeof(begin_file_write)), sender);

    REQUIRE(protocol.__get_state() == State::FileTransfer);
    REQUIRE(file_ops.file_is_open == true);
    REQUIRE(file_ops.byte_count == 0);
    REQUIRE(sender.sent_tags.size() == 1);
    REQUIRE(sender.sent_tags[0] == Protocol::Tag::Ack);

    sender.sent_tags.clear();

    const uint8_t byte_transfer[] = {0, 0x7D, 0x65, 0, 0, Protocol::FileBytes, 0, 0, 127};
    protocol.handle_chunk(sysex::Chunk(byte_transfer, sizeof(byte_transfer)), sender);
    REQUIRE(file_ops.byte_count == 2);
    REQUIRE(file_ops.content[0] == 127);
    REQUIRE(file_ops.content[1] == 0);
    REQUIRE(sender.sent_tags.size() == 1);
    REQUIRE(sender.sent_tags[0] == Protocol::Tag::Ack);

    sender.sent_tags.clear();

    const uint8_t end_write[] = {0, 0x7D, 0x65, 0, 0, Protocol::EndFileTransfer};
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

    const uint8_t sysex[9] = {syx_pack1(v1), syx_pack2(v1), syx_pack3(v1), v2, v2, v2,
                              syx_pack1(v3), syx_pack2(v3), syx_pack3(v3)};
    array<uint16_t, 9> bytes;
    const auto byte_count = sysex::codec::decode<9>(sysex, sysex + 9, bytes);

    REQUIRE(byte_count == 3);
    REQUIRE(bytes[0] == 100);
    REQUIRE(bytes[1] == 0);
    REQUIRE(bytes[2] == 127);
  }));
}

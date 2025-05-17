#include "test_support.h"

#include <cstddef>
#include <cstdint>

#include "etl/array.h"

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

    // TODO: Use Chunk instead
    constexpr size_t write(const etl::array<uint8_t, BlockSize> &bytes, const size_t count) {
      parent.content = bytes;
      parent.byte_count = count;
      return count;
    }
  };

  // Handle should close upon destruction
  constexpr Handle open(const char *path) {
    return Handle(*this);
  }

  bool file_is_open = false;
  size_t byte_count = 0;
  etl::array<uint8_t, BlockSize> content;
};

typedef sysex::Protocol<TestFileOps> Protocol;
typedef Protocol::State State;

TEST_CASE("Protocol with empty bytes") {
  CONST_BODY(({
    TestFileOps file_ops;
    Protocol protocol(file_ops);
    const uint8_t data[0] = {};
    sysex::Chunk chunk(data, 0);
    protocol.handle_chunk(chunk);
    REQUIRE(protocol.__get_state() == State::Idle);
  }));
}

TEST_CASE("Protocol receives file data") {
  CONST_BODY(({
    TestFileOps file_ops;
    REQUIRE(file_ops.file_is_open == false);
    Protocol protocol(file_ops);

    const uint8_t begin_file_write[10] = {midi::SystemExclusive,    0, 0x7D, 0x65, 0, 0,
                                          Protocol::BeginFileWrite, 0, 50,   50};
    protocol.handle_chunk(sysex::Chunk(begin_file_write, 10));

    REQUIRE(protocol.__get_state() == State::FileTransfer);
    REQUIRE(file_ops.file_is_open == true);
    REQUIRE(file_ops.byte_count == 0);

    const uint8_t byte_transfer[10] = {midi::SystemExclusive, 0, 0x7D, 0x65, 0, 0,
                                       Protocol::FileBytes,   0, 0,    127};
    protocol.handle_chunk(sysex::Chunk(byte_transfer, 10));
    REQUIRE(file_ops.byte_count == 2);
    REQUIRE(file_ops.content[0] == 127);
    REQUIRE(file_ops.content[1] == 0);

    const uint8_t end_write[7] = {midi::SystemExclusive,    0, 0x7D, 0x65, 0, 0,
                                  Protocol::EndFileTransfer};
    protocol.handle_chunk(sysex::Chunk(end_write, 7));
    REQUIRE(protocol.__get_state() == State::Idle);
    REQUIRE(file_ops.file_is_open == false);
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

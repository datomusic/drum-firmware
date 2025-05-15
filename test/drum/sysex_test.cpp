#include "test_support.h"

#include "etl/array.h"
#include "etl/string.h"

#include "drum/sysex/protocol.h"

using etl::array;

struct TestFileOps {
  typedef int Handle;
  typedef etl::string<64> Path;
  static const unsigned BlockSize = 128;

  // Handle should close upon destruction
  static Handle open(const Path &path) {
    return 0;
  }

  static size_t write(const Handle &handle, const etl::span<const uint8_t, BlockSize> &bytes) {
    return 0;
  }
};

typedef sysex::Protocol<TestFileOps> Protocol;
typedef Protocol::State State;

/*
typedef Protocol::FileOperations File;

static constexpr File::FileHandle
test_open(const Protocol::FileOperations::Path &path) {
  return 0;
}

static constexpr Protocol::FileOperations file_ops{.open = File::Open::create<test_open>()};
*/

TEST_CASE("Protocol with empty bytes") {
  CONST_BODY(({
    Protocol protocol;
    const uint8_t data[0] = {};
    sysex::Chunk chunk(data, 0);
    protocol.handle_chunk(chunk);
    REQUIRE(protocol.__get_state() == State::Idle);
  }));
}

TEST_CASE("Protocol identifies") {
  CONST_BODY(({
    Protocol protocol;
    REQUIRE(protocol.__get_state() == State::Idle);

    const uint8_t data[3] = {0, 0x7D, 0x65};
    sysex::Chunk chunk(data, 3);
    protocol.handle_chunk(chunk);

    REQUIRE(protocol.__get_state() == State::Idle);
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
    const auto byte_count = sysex::codec::decode<9>(sysex, bytes);

    REQUIRE(byte_count == 3);
    REQUIRE(bytes[0] == 100);
    REQUIRE(bytes[1] == 0);
    REQUIRE(bytes[2] == 127);
  }));
}

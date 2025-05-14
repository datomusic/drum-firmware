#include "test_support.h"

#include "drum/sysex/protocol.h"

typedef sysex::Protocol::State State;

TEST_CASE("Protocol with empty bytes") {
  CONST_BODY(({
    sysex::Protocol protocol;
    const uint8_t data[0] = {};
    sysex::Chunk chunk(data, 0);
    protocol.handle_chunk(chunk);
    REQUIRE(protocol.__get_state() == State::Idle);
  }));
}

TEST_CASE("Protocol identifies") {
  CONST_BODY(({
    sysex::Protocol protocol;
    REQUIRE(protocol.__get_state() == State::Idle);

    const uint8_t data[3] = {midi::SystemExclusive, 0x7D, 0x65};
    sysex::Chunk chunk(data, 3);
    protocol.handle_chunk(chunk);

    REQUIRE(protocol.__get_state() == State::Identified);
  }));
}

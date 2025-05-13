#include "test_support.h"

#include "drum/sysex/protocol.h"

typedef sysex::Protocol::State State;

TEST_CASE("Protocol identifies") {
  CONST_BODY(({
    sysex::Protocol protocol;
    REQUIRE(protocol.__get_state() == State::Idle);

    const uint8_t data[3] = {0, 0x7D, 0x65};
    sysex::Chunk chunk(data, 3);
    protocol.handle_chunk(chunk);

    REQUIRE(protocol.__get_state() == State::Identified);
  }));
}

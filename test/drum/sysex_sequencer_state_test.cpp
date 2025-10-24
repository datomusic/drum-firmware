#include "test_support.h"

#include <cstddef>
#include <cstdint>

#include "drum/sequencer_persistence.h"
#include "drum/sysex/sequencer_state_codec.h"
#include "etl/array.h"
#include "etl/span.h"

using drum::SequencerPersistentState;

TEST_CASE("Encode sequencer state") {
  CONST_BODY(({
    SequencerPersistentState state;

    constexpr size_t NUM_STEPS = 8;
    constexpr size_t VELOCITIES_SIZE = 32;

    state.tracks[0].velocities[0] = 100;
    state.tracks[0].velocities[1] = 80;
    state.tracks[1].velocities[2] = 60;
    state.tracks[2].velocities[3] = 40;
    state.tracks[3].velocities[7] = 127;

    state.active_notes[0] = 37;
    state.active_notes[1] = 38;
    state.active_notes[2] = 46;
    state.active_notes[3] = 54;

    etl::array<uint8_t, sysex::SEQUENCER_STATE_PAYLOAD_SIZE> payload;
    const size_t encoded_size =
        sysex::encode_sequencer_state(state, etl::span{payload});

    REQUIRE(encoded_size == sysex::SEQUENCER_STATE_PAYLOAD_SIZE);

    REQUIRE(payload[0 * NUM_STEPS + 0] == 100);
    REQUIRE(payload[0 * NUM_STEPS + 1] == 80);
    REQUIRE(payload[0 * NUM_STEPS + 2] == 0);
    REQUIRE(payload[1 * NUM_STEPS + 2] == 60);
    REQUIRE(payload[2 * NUM_STEPS + 3] == 40);
    REQUIRE(payload[3 * NUM_STEPS + 7] == 127);

    REQUIRE(payload[VELOCITIES_SIZE + 0] == 37);
    REQUIRE(payload[VELOCITIES_SIZE + 1] == 38);
    REQUIRE(payload[VELOCITIES_SIZE + 2] == 46);
    REQUIRE(payload[VELOCITIES_SIZE + 3] == 54);
  }));
}

TEST_CASE("Decode sequencer state") {
  CONST_BODY(({
    constexpr size_t NUM_STEPS = 8;
    constexpr size_t VELOCITIES_SIZE = 32;

    etl::array<uint8_t, sysex::SEQUENCER_STATE_PAYLOAD_SIZE> payload;
    payload.fill(0);

    payload[0 * NUM_STEPS + 0] = 100;
    payload[0 * NUM_STEPS + 1] = 80;
    payload[1 * NUM_STEPS + 2] = 60;
    payload[2 * NUM_STEPS + 3] = 40;
    payload[3 * NUM_STEPS + 7] = 127;

    payload[VELOCITIES_SIZE + 0] = 37;
    payload[VELOCITIES_SIZE + 1] = 38;
    payload[VELOCITIES_SIZE + 2] = 46;
    payload[VELOCITIES_SIZE + 3] = 54;

    const auto maybe_state =
        sysex::decode_sequencer_state(etl::span<const uint8_t>{payload});

    REQUIRE(maybe_state.has_value());

    const auto &state = maybe_state.value();
    REQUIRE(state.tracks[0].velocities[0] == 100);
    REQUIRE(state.tracks[0].velocities[1] == 80);
    REQUIRE(state.tracks[1].velocities[2] == 60);
    REQUIRE(state.tracks[2].velocities[3] == 40);
    REQUIRE(state.tracks[3].velocities[7] == 127);

    REQUIRE(state.active_notes[0] == 37);
    REQUIRE(state.active_notes[1] == 38);
    REQUIRE(state.active_notes[2] == 46);
    REQUIRE(state.active_notes[3] == 54);
  }));
}

TEST_CASE("Encode and decode roundtrip") {
  CONST_BODY(({
    SequencerPersistentState original_state;

    for (size_t track = 0; track < 4; ++track) {
      for (size_t step = 0; step < 8; ++step) {
        original_state.tracks[track].velocities[step] =
            static_cast<uint8_t>((track * 8 + step) * 3);
      }
    }

    original_state.active_notes[0] = 37;
    original_state.active_notes[1] = 38;
    original_state.active_notes[2] = 46;
    original_state.active_notes[3] = 54;

    etl::array<uint8_t, sysex::SEQUENCER_STATE_PAYLOAD_SIZE> payload;
    sysex::encode_sequencer_state(original_state, etl::span{payload});

    const auto maybe_decoded =
        sysex::decode_sequencer_state(etl::span<const uint8_t>{payload});

    REQUIRE(maybe_decoded.has_value());

    const auto &decoded_state = maybe_decoded.value();

    for (size_t track = 0; track < 4; ++track) {
      for (size_t step = 0; step < 8; ++step) {
        REQUIRE(decoded_state.tracks[track].velocities[step] ==
                original_state.tracks[track].velocities[step]);
      }
    }

    for (size_t track = 0; track < 4; ++track) {
      REQUIRE(decoded_state.active_notes[track] ==
              original_state.active_notes[track]);
    }
  }));
}

TEST_CASE("Decode with invalid velocity") {
  CONST_BODY(({
    etl::array<uint8_t, sysex::SEQUENCER_STATE_PAYLOAD_SIZE> payload;
    payload.fill(0);

    payload[0] = 128;

    const auto maybe_state =
        sysex::decode_sequencer_state(etl::span<const uint8_t>{payload});

    REQUIRE(!maybe_state.has_value());
  }));
}

TEST_CASE("Decode with invalid note") {
  CONST_BODY(({
    constexpr size_t VELOCITIES_SIZE = 32;

    etl::array<uint8_t, sysex::SEQUENCER_STATE_PAYLOAD_SIZE> payload;
    payload.fill(0);

    payload[VELOCITIES_SIZE + 0] = 128;

    const auto maybe_state =
        sysex::decode_sequencer_state(etl::span<const uint8_t>{payload});

    REQUIRE(!maybe_state.has_value());
  }));
}

TEST_CASE("Decode with insufficient data") {
  CONST_BODY(({
    etl::array<uint8_t, 10> payload;
    payload.fill(0);

    const auto maybe_state =
        sysex::decode_sequencer_state(etl::span<const uint8_t>{payload});

    REQUIRE(!maybe_state.has_value());
  }));
}

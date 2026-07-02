#ifndef DRUM_SYSEX_SEQUENCER_STATE_CODEC_H
#define DRUM_SYSEX_SEQUENCER_STATE_CODEC_H

#include "drum/config.h"
#include "drum/sequencer_persistence.h"
#include "etl/array.h"
#include "etl/optional.h"
#include "etl/span.h"
#include <cstdint>

namespace sysex {

/**
 * @brief Codec for encoding/decoding sequencer state to/from SysEx messages.
 *
 * Wire payload format (37 bytes total, all 7-bit safe):
 * - 1 byte:  payload format version (currently SEQUENCER_STATE_PAYLOAD_VERSION)
 * - 32 bytes: velocities for all steps (4 tracks x 8 steps), track-major
 *   Layout: [T0S0, T0S1, ..., T0S7, T1S0, T1S1, ..., T3S7]
 * - 4 bytes: active notes per track [T0, T1, T2, T3]
 *
 * All values are already MIDI-compliant (0-127); SysEx transport already
 * constrains every byte to 7 bits, so no additional range validation is
 * needed here.
 *
 * NOTE: If the payload layout ever changes, bump
 * SEQUENCER_STATE_PAYLOAD_VERSION so old and new firmware/clients can detect
 * a mismatch instead of misinterpreting bytes.
 */

static constexpr uint8_t SEQUENCER_STATE_PAYLOAD_VERSION = 1;
static constexpr size_t SEQUENCER_STATE_VERSION_SIZE = 1;

static constexpr size_t SEQUENCER_STATE_DATA_SIZE =
    (drum::config::NUM_TRACKS * drum::config::NUM_STEPS_PER_TRACK) +
    drum::config::NUM_TRACKS;

static constexpr size_t SEQUENCER_STATE_PAYLOAD_SIZE =
    SEQUENCER_STATE_VERSION_SIZE + SEQUENCER_STATE_DATA_SIZE;

/**
 * @brief Encodes sequencer state into a versioned, 7-bit safe SysEx payload.
 *
 * @param state The sequencer state to encode
 * @param output Output buffer for encoded data (must be at least
 * SEQUENCER_STATE_PAYLOAD_SIZE)
 * @return Number of bytes written to output, or 0 if the output buffer is
 * too small
 */
inline size_t
encode_sequencer_state(const drum::SequencerPersistentState &state,
                       etl::span<uint8_t> output) {
  if (output.size() < SEQUENCER_STATE_PAYLOAD_SIZE) {
    return 0;
  }

  size_t pos = 0;

  output[pos++] = SEQUENCER_STATE_PAYLOAD_VERSION;

  for (size_t track = 0; track < drum::config::NUM_TRACKS; ++track) {
    for (size_t step = 0; step < drum::config::NUM_STEPS_PER_TRACK; ++step) {
      output[pos++] = state.tracks[track].velocities[step] & 0x7F;
    }
  }

  for (size_t track = 0; track < drum::config::NUM_TRACKS; ++track) {
    output[pos++] = state.active_notes[track] & 0x7F;
  }

  return pos;
}

/**
 * @brief Decodes a versioned SysEx payload into sequencer state.
 *
 * Rejects payloads that are too short or whose version byte does not match
 * the version this firmware understands.
 *
 * @param input SysEx payload data (including the leading version byte)
 * @return Decoded state if valid, nullopt if payload is invalid or the
 * version is unsupported
 */
inline etl::optional<drum::SequencerPersistentState>
decode_sequencer_state(const etl::span<const uint8_t> &input) {
  if (input.size() < SEQUENCER_STATE_PAYLOAD_SIZE) {
    return etl::nullopt;
  }

  size_t pos = 0;

  const uint8_t version = input[pos++];
  if (version != SEQUENCER_STATE_PAYLOAD_VERSION) {
    return etl::nullopt;
  }

  drum::SequencerPersistentState state;

  for (size_t track = 0; track < drum::config::NUM_TRACKS; ++track) {
    for (size_t step = 0; step < drum::config::NUM_STEPS_PER_TRACK; ++step) {
      state.tracks[track].velocities[step] = input[pos++];
    }
  }

  for (size_t track = 0; track < drum::config::NUM_TRACKS; ++track) {
    state.active_notes[track] = input[pos++];
  }

  return state;
}

} // namespace sysex

#endif // DRUM_SYSEX_SEQUENCER_STATE_CODEC_H

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
 * Message format (36 bytes total, all 7-bit safe):
 * - 32 bytes: velocities for all steps (4 tracks × 8 steps)
 *   Layout: [T0S0, T0S1, ..., T0S7, T1S0, T1S1, ..., T3S7]
 * - 4 bytes: active notes per track [T0, T1, T2, T3]
 *
 * All values are already MIDI-compliant (0-127), no additional encoding needed.
 */

static constexpr size_t SEQUENCER_STATE_PAYLOAD_SIZE =
    (drum::config::NUM_TRACKS * drum::config::NUM_STEPS_PER_TRACK) +
    drum::config::NUM_TRACKS;

/**
 * @brief Encodes sequencer state into a 7-bit safe SysEx payload.
 *
 * @param state The sequencer state to encode
 * @param output Output buffer for encoded data (must be at least
 * SEQUENCER_STATE_PAYLOAD_SIZE)
 * @return Number of bytes written to output
 */
inline size_t
encode_sequencer_state(const drum::SequencerPersistentState &state,
                       etl::span<uint8_t> output) {
  if (output.size() < SEQUENCER_STATE_PAYLOAD_SIZE) {
    return 0;
  }

  size_t pos = 0;

  // Encode velocities (32 bytes: 4 tracks × 8 steps)
  for (size_t track = 0; track < drum::config::NUM_TRACKS; ++track) {
    for (size_t step = 0; step < drum::config::NUM_STEPS_PER_TRACK; ++step) {
      output[pos++] = state.tracks[track].velocities[step] & 0x7F;
    }
  }

  // Encode active notes (4 bytes)
  for (size_t track = 0; track < drum::config::NUM_TRACKS; ++track) {
    output[pos++] = state.active_notes[track] & 0x7F;
  }

  return pos;
}

/**
 * @brief Decodes a SysEx payload into sequencer state.
 *
 * @param input SysEx payload data
 * @return Decoded state if valid, nullopt if payload is invalid
 */
inline etl::optional<drum::SequencerPersistentState>
decode_sequencer_state(const etl::span<const uint8_t> &input) {
  if (input.size() < SEQUENCER_STATE_PAYLOAD_SIZE) {
    return etl::nullopt;
  }

  drum::SequencerPersistentState state;
  size_t pos = 0;

  // Decode velocities (32 bytes: 4 tracks × 8 steps)
  for (size_t track = 0; track < drum::config::NUM_TRACKS; ++track) {
    for (size_t step = 0; step < drum::config::NUM_STEPS_PER_TRACK; ++step) {
      const uint8_t velocity = input[pos++];
      // Validate 7-bit MIDI value
      if (velocity > 127) {
        return etl::nullopt;
      }
      state.tracks[track].velocities[step] = velocity;
    }
  }

  // Decode active notes (4 bytes)
  for (size_t track = 0; track < drum::config::NUM_TRACKS; ++track) {
    const uint8_t note = input[pos++];
    // Validate 7-bit MIDI note
    if (note > 127) {
      return etl::nullopt;
    }
    state.active_notes[track] = note;
  }

  return state;
}

} // namespace sysex

#endif // DRUM_SYSEX_SEQUENCER_STATE_CODEC_H

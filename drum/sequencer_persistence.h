#ifndef DRUM_SEQUENCER_PERSISTENCE_H
#define DRUM_SEQUENCER_PERSISTENCE_H

#include "config.h"
#include "etl/array.h"
#include <cstdint>

namespace drum {

/**
 * @brief Data structure for persisting sequencer state to flash storage.
 *
 * Contains all necessary state to restore the main sequencer at boot:
 * - Per-step velocities (0 = disabled)
 * - Active MIDI note numbers for each track (used for all enabled steps)
 *
 * Only the main sequencer is saved - variation and random sequencers
 * are generated on-the-fly and don't need persistence.
 */
struct SequencerPersistentState {
  // File format version and validation
  static constexpr uint32_t MAGIC_NUMBER = 0x53455143; // 'SEQC'
  // v2 drops per-step notes; relies on per-track active note.
  static constexpr uint8_t FORMAT_VERSION = 2;

  uint32_t magic;
  uint8_t version;
  uint8_t reserved[3]; // Padding for alignment

  // Sequencer pattern data - main sequencer only
  struct TrackData {
    // Only store velocity per step; 0 means disabled.
    etl::array<uint8_t, config::NUM_STEPS_PER_TRACK> velocities;
  };
  etl::array<TrackData, config::NUM_TRACKS> tracks;

  // Active note assignments per track (for drumpad triggering)
  etl::array<uint8_t, config::NUM_TRACKS> active_notes;

  SequencerPersistentState() : magic(MAGIC_NUMBER), version(FORMAT_VERSION) {
    // Initialize active notes with first note from each track's range
    active_notes[0] = config::track_0_notes[0]; // Kick
    active_notes[1] = config::track_1_notes[0]; // Snare
    active_notes[2] = config::track_2_notes[0]; // Percussion
    active_notes[3] = config::track_3_notes[0]; // Hi-Hat

    // Clear all pattern data
    for (auto &track : tracks) {
      track.velocities.fill(0);
    }
  }

  /**
   * @brief Validates the loaded data structure.
   * @return true if valid, false if corrupted or unsupported version
   */
  bool is_valid() const {
    return magic == MAGIC_NUMBER && version == FORMAT_VERSION;
  }
};

// Compile-time size check to ensure we're not too large
static_assert(sizeof(SequencerPersistentState) < 512,
              "SequencerPersistentState too large for efficient flash storage");

} // namespace drum

#endif // DRUM_SEQUENCER_PERSISTENCE_H

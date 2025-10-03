#ifndef MUSIN_TIMING_TIMING_CONSTANTS_H
#define MUSIN_TIMING_TIMING_CONSTANTS_H

#include <cstdint>

namespace musin::timing {

/**
 * @brief Default Pulses Per Quarter Note (PPQN) used for sequencer timing.
 * Set to 12 PPQN for efficient processing while maintaining musical resolution.
 * Raw clock sources (MIDI, SYNC) operate at 24 PPQN and are downsampled by
 * SpeedAdapter.
 */
constexpr uint32_t DEFAULT_PPQN = 12;

// Common 12-PPQN phase markers for clarity at call sites
constexpr uint8_t PHASE_DOWNBEAT = 0;                      // quarter start
constexpr uint8_t PHASE_EIGHTH_OFFBEAT = DEFAULT_PPQN / 2; // 6
constexpr uint8_t PHASE_TRIPLET_STEP = DEFAULT_PPQN / 3;   // 4
constexpr uint8_t PHASE_SIXTEENTH_STEP = DEFAULT_PPQN / 4; // 3

} // namespace musin::timing

#endif // MUSIN_TIMING_TIMING_CONSTANTS_H

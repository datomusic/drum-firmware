#ifndef MUSIN_TIMING_TIMING_CONSTANTS_H
#define MUSIN_TIMING_TIMING_CONSTANTS_H

#include <cstdint>

namespace musin::timing {

/**
 * @brief Default Pulses Per Quarter Note (PPQN) used for high-resolution
 * timing. Standard MIDI clock is 24, common sequencer resolution is 96.
 */
constexpr uint32_t DEFAULT_PPQN = 24;

// Common 24-PPQN phase markers for clarity at call sites
constexpr uint8_t PHASE_DOWNBEAT = 0;                      // quarter start
constexpr uint8_t PHASE_EIGHTH_OFFBEAT = DEFAULT_PPQN / 2; // 12
constexpr uint8_t PHASE_TRIPLET_STEP = DEFAULT_PPQN / 3;   // 8
constexpr uint8_t PHASE_SIXTEENTH_STEP = DEFAULT_PPQN / 4; // 6

} // namespace musin::timing

#endif // MUSIN_TIMING_TIMING_CONSTANTS_H

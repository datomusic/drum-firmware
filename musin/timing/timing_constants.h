#ifndef MUSIN_TIMING_TIMING_CONSTANTS_H
#define MUSIN_TIMING_TIMING_CONSTANTS_H

#include <cstdint>

namespace musin::timing {

/**
 * @brief Default Pulses Per Quarter Note (PPQN) used for high-resolution timing.
 * Standard MIDI clock is 24, common sequencer resolution is 96.
 */
constexpr uint32_t DEFAULT_PPQN = 96;

} // namespace musin::timing

#endif // MUSIN_TIMING_TIMING_CONSTANTS_H

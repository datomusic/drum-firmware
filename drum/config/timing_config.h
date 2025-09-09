#ifndef DRUM_CONFIG_TIMING_CONFIG_H
#define DRUM_CONFIG_TIMING_CONFIG_H

#include <cstdint>

namespace drum::config {

/**
 * @brief Swing presets define the off-beat phase for second eighth note.
 * 
 * In a 24 PPQN system, these values represent the phase (0-23) where
 * the second eighth note occurs when swing is enabled.
 * 
 * - None (straight): 12  → 50/50 timing (default when swing is OFF)
 * - Light: 13  → ~54/46 timing
 * - Medium: 14 → ~58/42 timing  
 * - Strong: 15 → 62.5/37.5 timing
 * - FullShuffle: 16 → ~66.7/33.3 timing (≈ 2:1, default when swing is ON)
 */
enum class SwingPreset : uint8_t { 
  None = 12, 
  Light = 13, 
  Medium = 14, 
  Strong = 15, 
  FullShuffle = 16 
};

/**
 * @brief Compile-time selection of swing preset when swing is enabled.
 * This determines the off-beat phase used for sequencer timing.
 */
inline constexpr SwingPreset kSwingOnPreset = SwingPreset::FullShuffle;

} // namespace drum::config

#endif // DRUM_CONFIG_TIMING_CONFIG_H
#include "sequencer_effect_swing.h"
#include "drum/config.h"

namespace drum {

namespace {
// Musical timing constants
constexpr uint8_t PPQN = 24;
constexpr uint8_t DOWNBEAT = 0;
constexpr uint8_t STRAIGHT_OFFBEAT = PPQN / 2; // 12

// Retrigger masks for different subdivisions
constexpr uint32_t SIXTEENTH_MASK =
    (1u << 0) | (1u << 6) | (1u << 12) | (1u << 18);
constexpr uint32_t TRIPLET_MASK = (1u << 0) | (1u << 8) | (1u << 16);
constexpr uint32_t MASK24 = (1u << 24) - 1u;
} // namespace

SequencerEffectSwing::StepTiming SequencerEffectSwing::calculate_step_timing(
    size_t next_index, bool repeat_active, uint64_t transport_step) const {
  // Determine step parity for swing calculation
  // When repeat is active, use absolute transport step for consistent timing
  // When not repeating, use the pattern index for swing parity
  const bool next_is_even =
      repeat_active ? ((transport_step & 1u) == 0) : ((next_index & 1u) == 0);

  // Calculate base expected phase (straight timing)
  uint8_t expected_phase = next_is_even ? DOWNBEAT : STRAIGHT_OFFBEAT;

  // Apply swing delay if enabled and this step should be delayed
  bool is_delay_applied = false;
  if (swing_enabled_) {
    const bool should_delay = (swing_delays_odd_steps_ && !next_is_even) ||
                              (!swing_delays_odd_steps_ && next_is_even);
    if (should_delay) {
      expected_phase = static_cast<uint8_t>(
          (expected_phase + drum::config::timing::SWING_OFFSET_PHASES) % PPQN);
      is_delay_applied = true;
    }
  }

  // Select appropriate retrigger mask based on swing state
  const uint32_t base_mask = swing_enabled_ ? TRIPLET_MASK : SIXTEENTH_MASK;

  // Rotate the mask by the expected phase to align retriggers with the main
  // step
  const uint8_t rotation = expected_phase;
  const uint32_t substep_mask =
      ((base_mask << rotation) | (base_mask >> (24 - rotation))) & MASK24;

  return {expected_phase, substep_mask, is_delay_applied};
}

void SequencerEffectSwing::set_swing_enabled(bool enabled) {
  if (swing_enabled_ == enabled) {
    return;
  }

  swing_enabled_ = enabled;
}

bool SequencerEffectSwing::is_swing_enabled() const {
  return swing_enabled_;
}

void SequencerEffectSwing::set_swing_target(bool delay_odd) {
  if (swing_delays_odd_steps_ == delay_odd) {
    return;
  }

  swing_delays_odd_steps_ = delay_odd;
}

} // namespace drum

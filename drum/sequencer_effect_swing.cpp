#include "sequencer_effect_swing.h"
#include "drum/config.h"

namespace drum {

namespace {
// Musical timing constants
constexpr uint8_t PPQN = 12;
constexpr uint8_t DOWNBEAT = 0;
constexpr uint8_t STRAIGHT_OFFBEAT = PPQN / 2; // 6

// Retrigger masks for different subdivisions
constexpr uint32_t SIXTEENTH_MASK =
    (1u << 0) | (1u << 3) | (1u << 6) | (1u << 9);
constexpr uint32_t TRIPLET_MASK = (1u << 0) | (1u << 4) | (1u << 8);
constexpr uint32_t MASK12 = (1u << 12) - 1u;

constexpr uint32_t rotl12(uint32_t mask, uint8_t rotation) {
  return ((mask << rotation) | (mask >> (12 - rotation))) & MASK12;
}
} // namespace

uint8_t SequencerEffectSwing::onset_phase_for_parity(bool is_even) const {
  const uint8_t straight_phase = is_even ? DOWNBEAT : STRAIGHT_OFFBEAT;
  const bool is_delayed =
      swing_enabled_ && (swing_delays_odd_steps_ ? !is_even : is_even);
  if (is_delayed) {
    return static_cast<uint8_t>(
        (straight_phase + drum::config::timing::SWING_OFFSET_PHASES) % PPQN);
  }
  return straight_phase;
}

SequencerEffectSwing::StepTiming SequencerEffectSwing::calculate_step_timing(
    size_t next_index, bool repeat_active, uint64_t transport_step) const {
  // Determine step parity for swing calculation
  // When repeat is active, use absolute transport step for consistent timing
  // When not repeating, use the pattern index for swing parity
  const bool next_is_even =
      repeat_active ? ((transport_step & 1u) == 0) : ((next_index & 1u) == 0);

  const uint8_t expected_phase = onset_phase_for_parity(next_is_even);
  const uint8_t straight_phase = next_is_even ? DOWNBEAT : STRAIGHT_OFFBEAT;
  const bool is_delay_applied = expected_phase != straight_phase;

  // Select appropriate retrigger mask based on swing state
  const uint32_t base_mask = swing_enabled_ ? TRIPLET_MASK : SIXTEENTH_MASK;

  // Rotate the mask by the expected phase to align retriggers with the main
  // step
  const uint32_t substep_mask = rotl12(base_mask, expected_phase);

  return {expected_phase, substep_mask, is_delay_applied};
}

SequencerEffectSwing::HitZone
SequencerEffectSwing::classify_hit_phase(uint8_t phase_12) const {
  // Zone masks anchored at phase 0: Early covers the ticks at and just after
  // an onset, Late covers the ticks just before it.
  constexpr uint32_t EARLY_BASE = (1u << EARLY_ZONE_TICKS) - 1u;
  constexpr uint32_t LATE_BASE = ((1u << LATE_ZONE_TICKS) - 1u)
                                 << (PPQN - LATE_ZONE_TICKS);

  const uint8_t even_onset = onset_phase_for_parity(true);
  const uint8_t odd_onset = onset_phase_for_parity(false);

  const uint32_t early_mask =
      rotl12(EARLY_BASE, even_onset) | rotl12(EARLY_BASE, odd_onset);
  const uint32_t late_mask =
      rotl12(LATE_BASE, even_onset) | rotl12(LATE_BASE, odd_onset);

  const uint32_t phase_bit = 1u << (phase_12 % PPQN);
  if ((early_mask & phase_bit) != 0u) {
    return HitZone::Early;
  }
  if ((late_mask & phase_bit) != 0u) {
    return HitZone::Late;
  }
  return HitZone::Middle;
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

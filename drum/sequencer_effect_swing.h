#ifndef DRUM_SEQUENCER_EFFECT_SWING_H
#define DRUM_SEQUENCER_EFFECT_SWING_H

#include <cstddef>
#include <cstdint>

namespace drum {

/**
 * @brief Encapsulates swing timing calculations for the sequencer.
 *
 * This class handles the timing policy for swing/groove effects, calculating
 * when steps should occur and what retrigger masks to use. It separates the
 * timing calculation concern from the main sequencer orchestration.
 */
class SequencerEffectSwing {
public:
  /**
   * @brief Complete timing decision for a sequencer step.
   */
  struct StepTiming {
    uint8_t expected_phase; // Phase in 12 PPQN when step should occur
    uint32_t substep_mask;  // Rotated mask for retrigger scheduling
    bool is_delay_applied;  // True if swing delay was applied to this step
  };

  /**
   * @brief Calculate complete timing information for a step.
   * @param next_index The step index that will be played next
   * @param repeat_active Whether repeat mode is currently active
   * @param transport_step Current transport step counter for parity calculation
   * @return Complete timing decision including phase and retrigger mask
   */
  [[nodiscard]] StepTiming calculate_step_timing(size_t next_index,
                                                 bool repeat_active,
                                                 uint64_t transport_step) const;

  /**
   * @brief Enable or disable swing timing.
   * @param enabled true to enable swing, false for straight timing
   */
  void set_swing_enabled(bool enabled);

  /**
   * @brief Check if swing timing is currently enabled.
   * @return true if swing is enabled, false for straight timing
   */
  [[nodiscard]] bool is_swing_enabled() const;

  /**
   * @brief Set which steps receive swing delay.
   * @param delay_odd If true, odd steps are delayed. If false, even steps are
   * delayed.
   */
  void set_swing_target(bool delay_odd);

private:
  bool swing_enabled_{false};
  bool swing_delays_odd_steps_{true};
};

} // namespace drum

#endif // DRUM_SEQUENCER_EFFECT_SWING_H

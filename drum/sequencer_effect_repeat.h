#ifndef DRUM_SEQUENCER_EFFECT_REPEAT_H
#define DRUM_SEQUENCER_EFFECT_REPEAT_H

#include <cstddef>
#include <cstdint>

namespace drum {

/**
 * @brief Encapsulates the repeat (loop) effect for the sequencer.
 *
 * While active, playback loops over a window of steps starting at the step
 * that was playing when the effect was activated.
 */
class SequencerEffectRepeat {
public:
  /**
   * @brief Activate the repeat loop.
   * @param length Loop length in steps (clamped to at least 1).
   * @param current_step_counter The transport step counter at activation.
   * @param num_steps Number of steps in the pattern.
   */
  void activate(uint32_t length, uint32_t current_step_counter,
                size_t num_steps);

  void deactivate();

  /**
   * @brief Change the loop length while active. Ignored when inactive.
   */
  void set_length(uint32_t length);

  [[nodiscard]] bool is_active() const;

  /**
   * @brief The current loop length, or 0 when inactive.
   */
  [[nodiscard]] uint32_t get_length() const;

  /**
   * @brief Map the transport step counter to the pattern step to play,
   * looping within the repeat window while active.
   * @param step_counter The transport step counter.
   * @param num_steps Number of steps in the pattern (0 yields step 0).
   */
  [[nodiscard]] size_t calculate_step_index(uint32_t step_counter,
                                            size_t num_steps) const;

private:
  bool active_{false};
  uint32_t length_{0};
  uint32_t activation_step_index_{0};
  uint32_t activation_step_counter_{0};
};

} // namespace drum

#endif // DRUM_SEQUENCER_EFFECT_REPEAT_H

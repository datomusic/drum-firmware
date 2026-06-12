#include "sequencer_effect_repeat.h"

#include <algorithm>

namespace drum {

void SequencerEffectRepeat::activate(uint32_t length,
                                     uint32_t current_step_counter,
                                     size_t num_steps) {
  if (active_) {
    return;
  }
  active_ = true;
  length_ = std::max(uint32_t{1}, length);
  activation_step_index_ =
      (num_steps > 0) ? (current_step_counter % num_steps) : 0;
  activation_step_counter_ = current_step_counter;
}

void SequencerEffectRepeat::deactivate() {
  active_ = false;
  length_ = 0;
}

void SequencerEffectRepeat::set_length(uint32_t length) {
  if (active_) {
    length_ = std::max(uint32_t{1}, length);
  }
}

bool SequencerEffectRepeat::is_active() const {
  return active_;
}

uint32_t SequencerEffectRepeat::get_length() const {
  return active_ ? length_ : 0;
}

size_t SequencerEffectRepeat::calculate_step_index(uint32_t step_counter,
                                                   size_t num_steps) const {
  if (num_steps == 0) {
    return 0;
  }
  if (active_ && length_ > 0) {
    uint32_t steps_since_activation = step_counter - activation_step_counter_;
    uint32_t loop_position = steps_since_activation % length_;
    return (activation_step_index_ + loop_position) % num_steps;
  }
  return step_counter % num_steps;
}

} // namespace drum

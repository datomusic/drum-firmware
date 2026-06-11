#include "sequencer_effect_retrigger.h"

namespace drum {

void SequencerEffectRetrigger::set_mode(uint8_t track_index,
                                        RetriggerMode mode) {
  if (track_index < MAX_TRACKS) {
    mode_per_track_[track_index] = mode;
  }
}

RetriggerMode SequencerEffectRetrigger::get_mode(uint8_t track_index) const {
  if (track_index < MAX_TRACKS) {
    return mode_per_track_[track_index];
  }
  return RetriggerMode::Off;
}

void SequencerEffectRetrigger::mark_due_tracks(bool step_is_due,
                                               bool substep_is_due) {
  uint8_t mask = 0;
  for (size_t track_idx = 0; track_idx < MAX_TRACKS; ++track_idx) {
    RetriggerMode mode = mode_per_track_[track_idx];
    bool due = (mode == RetriggerMode::Step)       ? step_is_due
               : (mode == RetriggerMode::Substeps) ? substep_is_due
                                                   : false;
    if (due) {
      mask |= static_cast<uint8_t>(1u << track_idx);
    }
  }
  if (mask != 0) {
    due_mask_.fetch_or(mask, std::memory_order_relaxed);
  }
}

uint8_t SequencerEffectRetrigger::take_due_mask() {
  uint8_t mask = due_mask_.load(std::memory_order_relaxed);
  if (mask != 0) {
    due_mask_.fetch_and(static_cast<uint8_t>(~mask),
                        std::memory_order_relaxed);
  }
  return mask;
}

} // namespace drum

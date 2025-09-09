#include "save_timing_manager.h"
#include "pico/time.h"

namespace drum {

SaveTimingManager::SaveTimingManager(TimeSource &time_source,
                                     uint32_t save_debounce_ms,
                                     uint32_t max_save_interval_ms)
    : time_source_(time_source), save_debounce_ms_(save_debounce_ms),
      max_save_interval_ms_(max_save_interval_ms) {
}

void SaveTimingManager::mark_dirty() {
  state_is_dirty_ = true;
  last_change_time_ms_ = time_source_.get_time_ms();
}

void SaveTimingManager::mark_clean() {
  state_is_dirty_ = false;
  last_save_time_ms_ = time_source_.get_time_ms();
}

bool SaveTimingManager::is_dirty() const {
  return state_is_dirty_;
}

bool SaveTimingManager::should_save_now() const {
  if (!state_is_dirty_) {
    return false;
  }

  uint32_t current_time_ms = time_source_.get_time_ms();
  uint32_t time_since_change = current_time_ms - last_change_time_ms_;
  uint32_t time_since_save = current_time_ms - last_save_time_ms_;

  // Save if enough time has passed since last change (debounce)
  // OR if maximum interval has been exceeded
  return (time_since_change >= save_debounce_ms_ ||
          time_since_save >= max_save_interval_ms_);
}

uint32_t PicoTimeSource::get_time_ms() const {
  return time_us_32() / 1000;
}

} // namespace drum
#ifndef DRUM_SAVE_TIMING_MANAGER_H
#define DRUM_SAVE_TIMING_MANAGER_H

#include <cstdint>

namespace drum {

/**
 * @brief Abstract time source for dependency injection.
 */
class TimeSource {
public:
  virtual ~TimeSource() = default;
  virtual uint32_t get_time_ms() const = 0;
};

/**
 * @brief Manages save timing and debounce logic.
 * 
 * This class handles the debounce logic for determining when state
 * should be saved, with an injectable time source for testability.
 */
class SaveTimingManager {
public:
  /**
   * @brief Constructor with injectable time source.
   * @param time_source The time source to use for timing decisions
   * @param save_debounce_ms Minimum time between state changes and save
   * @param max_save_interval_ms Maximum time between saves when dirty
   */
  SaveTimingManager(TimeSource& time_source, 
                   uint32_t save_debounce_ms = 2000,
                   uint32_t max_save_interval_ms = 30000);

  /**
   * @brief Mark that state has changed (needs saving).
   */
  void mark_dirty();

  /**
   * @brief Mark that state has been saved (clean).
   */
  void mark_clean();

  /**
   * @brief Check if state needs saving.
   * @return true if state is dirty, false otherwise
   */
  bool is_dirty() const;

  /**
   * @brief Check if state should be saved now based on debounce logic.
   * @return true if state should be saved now, false otherwise
   */
  bool should_save_now() const;

private:
  TimeSource& time_source_;
  uint32_t save_debounce_ms_;
  uint32_t max_save_interval_ms_;
  
  bool state_is_dirty_ = false;
  uint32_t last_change_time_ms_ = 0;
  uint32_t last_save_time_ms_ = 0;
};

/**
 * @brief Pico SDK time source implementation.
 */
class PicoTimeSource : public TimeSource {
public:
  uint32_t get_time_ms() const override;
};

} // namespace drum

#endif // DRUM_SAVE_TIMING_MANAGER_H
#ifndef DRUM_SEQUENCER_EFFECT_RETRIGGER_H
#define DRUM_SEQUENCER_EFFECT_RETRIGGER_H

#include "etl/array.h"
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace drum {

enum class RetriggerMode : uint8_t {
  Off = 0,
  Step = 1,
  Substeps = 2
};

/**
 * @brief Encapsulates per-track retriggering ("play on every step").
 *
 * Tracks set to Step mode retrigger on every step boundary; Substeps mode
 * retriggers on the substep grid as well. Due tracks are accumulated into an
 * atomic mask from the tempo callback (mark_due_tracks) and drained from the
 * main loop (take_due_mask).
 */
class SequencerEffectRetrigger {
public:
  static constexpr size_t MAX_TRACKS = 4;

  void set_mode(uint8_t track_index, RetriggerMode mode);

  [[nodiscard]] RetriggerMode get_mode(uint8_t track_index) const;

  /**
   * @brief Accumulate due tracks for the elapsed tick window.
   * Safe to call from the tempo notification context.
   * @param step_is_due Whether a step boundary fell in the window.
   * @param substep_is_due Whether a substep grid point fell in the window.
   */
  void mark_due_tracks(bool step_is_due, bool substep_is_due);

  /**
   * @brief Drain and return the mask of tracks due for a retrigger.
   * Call from the main loop; bit N set means track N is due.
   */
  uint8_t take_due_mask();

private:
  etl::array<RetriggerMode, MAX_TRACKS> mode_per_track_{};
  std::atomic<uint8_t> due_mask_{0};
};

} // namespace drum

#endif // DRUM_SEQUENCER_EFFECT_RETRIGGER_H

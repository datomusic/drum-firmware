#ifndef SB25_DRUM_SEQUENCER_CONTROLLER_H
#define SB25_DRUM_SEQUENCER_CONTROLLER_H

#include "etl/observer.h"
#include "sequencer_tick_event.h"
#include "step_sequencer.h" // Include the actual sequencer definition

namespace StepSequencer {

// Forward declare the specific Sequencer instantiation used
template <size_t NumTracks, size_t NumSteps> class Sequencer;

/**
 * @brief Receives sequencer ticks and advances the main sequencer.
 *
 * Acts as the bridge between the tempo generation system (TempoMultiplier)
 * and the musical pattern storage (Sequencer).
 */
class SequencerController : public etl::observer<Tempo::SequencerTickEvent> {
public:
  /**
   * @brief Constructor.
   * @param sequencer_ref A reference to the main Sequencer instance.
   */
  explicit SequencerController(StepSequencer::Sequencer<4, 8> &sequencer_ref);

  // Prevent copying and assignment
  SequencerController(const SequencerController &) = delete;
  SequencerController &operator=(const SequencerController &) = delete;

  /**
   * @brief Notification handler called when a SequencerTickEvent is received.
   * Implements the etl::observer interface.
   * @param event The received sequencer tick event.
   */
  void notification(const Tempo::SequencerTickEvent &event);

  /**
   * @brief Get the current step index within the pattern length.
   */
   [[nodiscard]] uint32_t get_current_step() const;

  /**
   * @brief Reset the current step index (e.g., on transport stop/start).
   */
  void reset();

private:
  StepSequencer::Sequencer<4, 8> &sequencer; // Reference to the actual sequencer
  uint32_t current_step_counter;             // Continuously running step counter
  // Add other state as needed (e.g., playing/stopped)
};

} // namespace StepSequencer

#endif // SB25_DRUM_SEQUENCER_CONTROLLER_H

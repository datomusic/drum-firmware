#ifndef SB25_DRUM_SEQUENCER_CONTROLLER_H
#define SB25_DRUM_SEQUENCER_CONTROLLER_H

#include "etl/array.h"
#include "etl/observer.h"
#include "sequencer_tick_event.h"
#include "step_sequencer.h" // Include the actual sequencer definition
#include <cstdint>          // Include for uint8_t
#include <optional>         // Include for std::optional

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
   * @param tempo_source_ref A reference to the observable that emits SequencerTickEvents.
   */
  explicit SequencerController(StepSequencer::Sequencer<4, 8> &sequencer_ref,
                              etl::observable<etl::observer<Tempo::SequencerTickEvent>, 2> &tempo_source_ref);

  // Prevent copying and assignment
  SequencerController(const SequencerController &) = delete;
  SequencerController &operator=(const SequencerController &) = delete;

  /**
   * @brief Notification handler called when a SequencerTickEvent is received.
   * Implements the etl::observer interface.
   * @param event The received sequencer tick event.
   */
  void notification(Tempo::SequencerTickEvent event);

  /**
   * @brief Get the current step index within the pattern length.
   */
  [[nodiscard]] uint32_t get_current_step() const;

  /**
   * @brief Reset the current step index (e.g., on transport stop/start).
   */
  void reset();

  /**
   * @brief Start the sequencer by connecting to the tempo source.
   * @return true if successfully started, false if already running
   */
  bool start();

  /**
   * @brief Stop the sequencer by disconnecting from the tempo source.
   * @return true if successfully stopped, false if already stopped
   */
  bool stop();

  /**
   * @brief Check if the sequencer is currently running.
   * @return true if running, false otherwise
   */
  [[nodiscard]] bool is_running() const;

private:
  StepSequencer::Sequencer<4, 8> &sequencer; // Reference to the actual sequencer
  uint32_t current_step_counter;             // Continuously running step counter
  etl::array<std::optional<uint8_t>, 4>
      last_played_note_per_track;                 // Store the last note number played on each track
  etl::array<int8_t, 4> track_offsets_{};         // Per-track step offsets
  uint8_t current_random_strength_ = 0;           // Current randomization strength (0-127)
  static constexpr uint8_t MAX_RANDOM_OFFSET = 3; // Max ±3 steps from base
  etl::observable<etl::observer<Tempo::SequencerTickEvent>, 2> &tempo_source;
  bool running = false;
};

} // namespace StepSequencer

#endif // SB25_DRUM_SEQUENCER_CONTROLLER_H

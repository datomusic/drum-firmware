#ifndef SB25_DRUM_SEQUENCER_CONTROLLER_H
#define SB25_DRUM_SEQUENCER_CONTROLLER_H

#include "etl/array.h"
#include "etl/observer.h"
#include "sequencer_tick_event.h"
#include "step_sequencer.h"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <optional>

#include <cstddef> // For size_t

// Forward declarations
class PizzaControls;

namespace StepSequencer {

// Forward declare the specific Sequencer instantiation used
template <size_t NumTracks, size_t NumSteps> class Sequencer;

/**
 * @brief Receives sequencer ticks and advances the main sequencer.
 *
 * Acts as the bridge between the tempo generation system (TempoMultiplier)
 * and the musical pattern storage (Sequencer). It operates on a high-resolution
 * internal clock tick derived from the tempo source.
 */
template <size_t NumTracks, size_t NumSteps>
class SequencerController : public etl::observer<Tempo::SequencerTickEvent> {
public:
  // --- Constants ---
  static constexpr uint32_t CLOCK_PPQN = 96;
  static constexpr uint8_t SEQUENCER_RESOLUTION = 16; // e.g., 16th notes

  /**
   * @brief Constructor.
   * @param sequencer_ref A reference to the main Sequencer instance.
   * @param tempo_source_ref A reference to the observable that emits SequencerTickEvents.
   */
  SequencerController(
      StepSequencer::Sequencer<NumTracks, NumSteps> &sequencer_ref,
      etl::observable<etl::observer<Tempo::SequencerTickEvent>, 2> &tempo_source_ref);
  ~SequencerController();

  SequencerController(const SequencerController &) = delete;
  SequencerController &operator=(const SequencerController &) = delete;

  /**
   * @brief Notification handler called when a SequencerTickEvent is received.
   * Implements the etl::observer interface. This is expected to be called
   * at the high resolution defined by CLOCK_PPQN.
   * @param event The received sequencer tick event.
   */
  void notification(Tempo::SequencerTickEvent event) override;

  /**
   * @brief Get the current logical step index (0 to NumSteps-1) that was last triggered.
   */
  [[nodiscard]] uint32_t get_current_step() const noexcept;

  /**
   * @brief Get the index of the step (0 to NumSteps-1) that was last triggered/played for a
   * specific track. This considers effects like Repeat and Random that might alter the played step
   * for that track. Returns std::nullopt if no step has been played for the track since the last
   * reset/trigger.
   */
  [[nodiscard]] std::optional<size_t> get_last_played_step_for_track(size_t track_idx) const;

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

  /**
   * @brief Toggles the sequencer between running and stopped states.
   */
  void toggle();

  /**
   * @brief Set the swing amount.
   * @param percent Percentage (50-75) of the two-step duration allocated to the
   *                first step of the pair determined by swing_delays_odd_steps_.
   *                50 means no swing. Clamped internally.
   */
  void set_swing_percent(uint8_t percent);

  /**
   * @brief Set whether swing delay applies to odd steps.
   * @param delay_odd If true, odd steps (1, 3, ...) are delayed/longer.
   *                  If false (default), even steps (0, 2, ...) are delayed/longer.
   */
  void set_swing_target(bool delay_odd);

  /**
   * @brief Activate the random step effect.
   */
  void activate_random();

  /**
   * @brief Deactivate the random step effect.
   */
  void deactivate_random();

  [[nodiscard]] bool is_random_active() const;

  /**
   * @brief Set the pointer to the PizzaControls instance for callbacks.
   */
  void set_controls_ptr(PizzaControls *ptr);

private:
  enum class State : uint8_t {
    Stopped,
    Running,
    Repeating
  };
  void set_state(State new_state);
  void calculate_timing_params();
  [[nodiscard]] size_t calculate_base_step_index() const;
  void process_track_step(size_t track_idx, size_t step_index_to_play);
  [[nodiscard]] uint32_t calculate_next_trigger_interval() const;

  StepSequencer::Sequencer<NumTracks, NumSteps> &sequencer;
  uint32_t current_step_counter;
  etl::array<std::optional<uint8_t>, NumTracks> last_played_note_per_track;
  etl::array<std::optional<size_t>, NumTracks> _just_played_step_per_track;
  etl::array<int8_t, NumTracks> track_offsets_{};
  etl::observable<etl::observer<Tempo::SequencerTickEvent>, 2> &tempo_source;
  State state_ = State::Stopped;

  // --- Swing Timing Members ---
  uint8_t swing_percent_ = 50;
  bool swing_delays_odd_steps_ = false;
  uint32_t high_res_ticks_per_step_ = 0;
  uint64_t high_res_tick_counter_ = 0;
  uint64_t next_trigger_tick_target_ = 0;

  // --- Repeat Effect Members ---
  bool repeat_active_ = false;
  uint32_t repeat_length_ = 0;
  uint32_t repeat_activation_step_index_ = 0;
  uint64_t repeat_activation_step_counter_ = 0;

  bool random_active_ = false;
  etl::array<int8_t, NumTracks> random_track_offsets_{};
  PizzaControls *_controls_ptr = nullptr; // Pointer for callbacks

public:
  void activate_repeat(uint32_t length);
  void deactivate_repeat();
  void set_repeat_length(uint32_t length);
  [[nodiscard]] bool is_repeat_active() const;
};

} // namespace StepSequencer

#endif // SB25_DRUM_SEQUENCER_CONTROLLER_H

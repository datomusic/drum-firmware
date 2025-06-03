#ifndef SB25_DRUM_SEQUENCER_CONTROLLER_H
#define SB25_DRUM_SEQUENCER_CONTROLLER_H

#include "etl/array.h"
#include "etl/observer.h"
#include "events.h"
#include "musin/timing/step_sequencer.h"
#include "musin/timing/tempo_event.h"   // Added
#include "musin/timing/tempo_handler.h" // Added for MAX_TEMPO_OBSERVERS
#include "musin/timing/timing_constants.h"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <optional>

#include "config.h" // For drum::config::sequencer_controller::MAX_NOTE_EVENT_OBSERVERS
#include <cstddef>  // For size_t

namespace drum {

// Forward declare the specific Sequencer instantiation from its new namespace
template <size_t NumTracks, size_t NumSteps> class Sequencer;

/**
 * @brief Receives sequencer ticks and advances the main sequencer.
 *
 * Acts as the bridge between the tempo generation system (TempoMultiplier)
 * and the musical pattern storage (Sequencer). It operates on a high-resolution
 * internal clock tick derived from the tempo source. Emits NoteEvents when steps play.
 */

template <size_t NumTracks, size_t NumSteps>
class SequencerController
    : public etl::observer<musin::timing::TempoEvent>,
      public etl::observable<etl::observer<drum::Events::NoteEvent>,
                             drum::config::sequencer_controller::MAX_NOTE_EVENT_OBSERVERS> {
public:
  static constexpr uint32_t CLOCK_PPQN = 24;
  static constexpr uint8_t SEQUENCER_RESOLUTION = 16; // e.g., 16th notes

  /**
   * @brief Constructor.
   * @param tempo_source_ref A reference to the observable that emits SequencerTickEvents.
   * @param sound_router_ref A reference to the SoundRouter instance.
   */
  SequencerController(musin::timing::TempoHandler &tempo_handler_ref);
  ~SequencerController();

  SequencerController(const SequencerController &) = delete;
  SequencerController &operator=(const SequencerController &) = delete;

  /**
   * @brief Notification handler called when a TempoEvent is received.
   * Implements the etl::observer interface. This is expected to be called
   * at the high resolution defined by CLOCK_PPQN.
   * @param event The received tempo event.
   */
  void notification(musin::timing::TempoEvent event) override; // Changed Event Type

  /**
   * @brief Triggers a note on event directly.
   * @param track_index The logical track index.
   * @param note The MIDI note number.
   * @param velocity The MIDI velocity.
   */
  void trigger_note_on(uint8_t track_index, uint8_t note, uint8_t velocity);

  /**
   * @brief Triggers a note off event directly.
   * @param track_index The logical track index.
   * @param note The MIDI note number.
   */
  void trigger_note_off(uint8_t track_index, uint8_t note);

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
   * Does not reset the step index.
   */
  void start();

  /**
   * @brief Stop the sequencer by disconnecting from the tempo source.
   */
  void stop();

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

  void set_random_probability(uint8_t percent) {
    random_probability_ = std::clamp(percent, static_cast<uint8_t>(0), static_cast<uint8_t>(100));
    printf("Probability set to %d\n", random_probability_);
  }

  [[nodiscard]] bool is_random_active() const;

  /**
   * @brief Sets the intended state of the repeat effect.
   * Compares the intended state with the current state and performs necessary actions
   * (activate, deactivate, set length).
   * @param intended_length std::nullopt to turn off repeat, or a value for the desired length.
   */
  void set_intended_repeat_state(std::optional<uint32_t> intended_length);

  /**
   * @brief Sets the active MIDI note number for a specific track.
   * This note is used by default when new steps are created or when drumpads are triggered.
   * @param track_index The logical track index.
   * @param note The MIDI note number to set as active for the track.
   */
  void set_active_note_for_track(uint8_t track_index, uint8_t note);

  /**
   * @brief Gets the currently active MIDI note number for a specific track.
   * @param track_index The logical track index.
   * @return The MIDI note number currently active for the track.
   */
  [[nodiscard]] uint8_t get_active_note_for_track(uint8_t track_index) const;

  void set_pad_pressed_state(uint8_t track_index, bool is_pressed);
  [[nodiscard]] bool is_pad_pressed(uint8_t track_index) const;

  /**
   * @brief Get the current retrigger mode for a track.
   * @param track_index The track index to check.
   * @return 0 for off, 1 for single retrigger, 2 for double retrigger.
   */
  [[nodiscard]] uint8_t get_retrigger_mode_for_track(uint8_t track_index) const;

  /**
   * @brief Get a reference to the internal sequencer instance.
   */
  [[nodiscard]] musin::timing::Sequencer<NumTracks, NumSteps> &get_sequencer() {
    return sequencer_;
  }
  /**
   * @brief Get a const reference to the internal sequencer instance.
   */
  [[nodiscard]] const musin::timing::Sequencer<NumTracks, NumSteps> &get_sequencer() const {
    return sequencer_;
  }

private:
  void calculate_timing_params();
  [[nodiscard]] size_t calculate_base_step_index() const;
  void process_track_step(size_t track_idx, size_t step_index_to_play);
  [[nodiscard]] uint32_t calculate_next_trigger_interval() const;

  musin::timing::Sequencer<NumTracks, NumSteps> sequencer_;
  uint32_t current_step_counter;
  etl::array<std::optional<uint8_t>, NumTracks> last_played_note_per_track;
  etl::array<std::optional<size_t>, NumTracks> _just_played_step_per_track; 
  musin::timing::TempoHandler &tempo_source;
  bool _running = false;

  uint8_t swing_percent_ = 50;
  bool swing_delays_odd_steps_ = false;
  uint32_t high_res_ticks_per_step_ = 0;
  uint64_t high_res_tick_counter_ = 0;
  uint64_t next_trigger_tick_target_ = 0;

  bool repeat_active_ = false;
  uint32_t repeat_length_ = 0;
  uint32_t repeat_activation_step_index_ = 0;
  uint64_t repeat_activation_step_counter_ = 0;

  etl::array<uint8_t, NumTracks> _retrigger_mode_per_track{};
  etl::array<uint32_t, NumTracks> _retrigger_progress_ticks_per_track{};

  bool random_active_ = false;
  uint8_t random_probability_ = drum::config::drumpad::RANDOM_PROBABILITY_DEFAULT;
  etl::array<int8_t, NumTracks> random_track_offsets_{};
  etl::array<uint8_t, NumTracks> _active_note_per_track{};
  etl::array<bool, NumTracks> _pad_pressed_state;

public:
  void activate_repeat(uint32_t length);
  void deactivate_repeat();
  /**
   * @brief Activates retriggering for a specific track.
   * @param track_index The track to activate retriggering on.
   * @param mode 1 for single retrigger per step, 2 for double retrigger per step.
   */
  void activate_play_on_every_step(uint8_t track_index, uint8_t mode);
  /**
   * @brief Deactivates retriggering for a specific track.
   * @param track_index The track to deactivate retriggering on.
   */
  void deactivate_play_on_every_step(uint8_t track_index);
  void set_repeat_length(uint32_t length);
  [[nodiscard]] bool is_repeat_active() const;

  /**
   * @brief Get the number of high-resolution SequencerTickEvents that form one musical step
   * (e.g., a 16th note) of this sequencer.
   */
  [[nodiscard]] uint32_t get_ticks_per_musical_step() const noexcept;
};

} // namespace drum

#endif // SB25_DRUM_SEQUENCER_CONTROLLER_H

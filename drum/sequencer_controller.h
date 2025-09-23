#ifndef DRUM_SEQUENCER_CONTROLLER_H
#define DRUM_SEQUENCER_CONTROLLER_H

#include "drum/config.h"
#include "etl/array.h"
#include "etl/observer.h"
#include "events.h"
#include "musin/timing/step_sequencer.h"
#include "musin/timing/tempo_event.h"
#include "musin/timing/tempo_handler.h"
#include "musin/timing/timing_constants.h"
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <optional>

#include "musin/hal/logger.h"
#include "randomness_provider.h"
#include "sequencer_effect_random.h"
#include "sequencer_effect_swing.h"
#include "sequencer_persistence.h"
#include "sequencer_storage.h"
#include <cstddef>

namespace drum {

enum class RetriggerMode : uint8_t {
  Off = 0,
  Step = 1,
  Substeps = 2
};

// Forward declare the specific Sequencer instantiation from its new namespace
template <size_t NumTracks, size_t NumSteps> class Sequencer;

/**
 * @brief Receives sequencer ticks and advances the main sequencer.
 *
 * Acts as the bridge between the tempo generation system (TempoMultiplier)
 * and the musical pattern storage (Sequencer). It operates on a high-resolution
 * internal clock tick derived from the tempo source. Emits NoteEvents when
 * steps play.
 */

template <size_t NumTracks, size_t NumSteps>
class SequencerController
    : public etl::observer<musin::timing::TempoEvent>,
      public etl::observer<drum::Events::SysExTransferStateChangeEvent>,
      public etl::observable<etl::observer<drum::Events::NoteEvent>,
                             drum::config::MAX_NOTE_EVENT_OBSERVERS> {
public:
  /**
   * @brief Constructor.
   * @param tempo_handler_ref A reference to the tempo handler.
   * @param logger A reference to the logger instance.
   */
  SequencerController(musin::timing::TempoHandler &tempo_handler_ref,
                      musin::Logger &logger);
  ~SequencerController();

  SequencerController(const SequencerController &) = delete;
  SequencerController &operator=(const SequencerController &) = delete;

  /**
   * @brief Notification handler called when a TempoEvent is received.
   * Implements the etl::observer interface. This is called on every
   * 24 PPQN tick with phase-based timing information.
   * @param event The received tempo event containing phase_24 (0-23).
   */
  void notification(musin::timing::TempoEvent event);

  /**
   * @brief Notification handler for SysEx transfer state changes.
   * @param event The event indicating the transfer state.
   */
  void notification(drum::Events::SysExTransferStateChangeEvent event);

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
   * @brief Get the current logical step index (0 to NumSteps-1) that was last
   * triggered.
   */
  [[nodiscard]] uint32_t get_current_step() const noexcept;

  /**
   * @brief Get the index of the step (0 to NumSteps-1) that was last
   * triggered/played for a specific track. This considers effects like Repeat
   * and Random that might alter the played step for that track. Returns
   * std::nullopt if no step has been played for the track since the last
   * reset/trigger.
   */
  [[nodiscard]] std::optional<size_t>
  get_last_played_step_for_track(size_t track_idx) const;

  /**
   * @brief Reset the current step index (e.g., on transport stop/start).
   */
  void reset();

  /**
   * @brief Immediately advance the sequencer to the next step.
   * Used for synchronization when external clock resumes after a timeout.
   */
  void advance_step();

  /**
   * @brief Start the sequencer by connecting to the tempo source.
   * Does not reset the step index.
   */
  void start();

  /**
   * @brief Checks for and processes a due sequencer step.
   * This should be called frequently from the main loop.
   */
  void update();

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
   * @brief Enable or disable swing timing.
   * When enabled, steps marked as "swung" are delayed by
   * config::timing::SWING_OFFSET_PHASES from the straight eighth anchors
   * (phases 0 and 12). When disabled, all steps use straight timing (0 and 12).
   * @param enabled true to enable swing, false for straight timing
   */
  void set_swing_enabled(bool enabled);

  /**
   * @brief Set whether swing delay applies to odd steps.
   * @param delay_odd If true, odd steps (1, 3, ...) are delayed (placed at
   *                  anchor + SWING_OFFSET_PHASES). If false, even steps
   *                  (0, 2, ...) are delayed.
   */
  void set_swing_target(bool delay_odd);

  /**
   * @brief Check if swing timing is currently enabled.
   * @return true if swing is enabled, false for straight timing
   */
  [[nodiscard]] bool is_swing_enabled() const;

  /**
   * @brief Start continuous 4-steps-ahead randomization.
   */
  void start_continuous_randomization();

  /**
   * @brief Stop continuous 4-steps-ahead randomization.
   */
  void stop_continuous_randomization();

  void set_random(float value);

  [[nodiscard]] bool is_continuous_randomization_active() const;

  void enable_random_offset_mode(float randomness_level);
  void disable_random_offset_mode();
  [[nodiscard]] bool is_random_offset_mode_active() const;
  void regenerate_random_offsets();

  void trigger_random_hard_press_behavior();
  void trigger_random_steps_when_stopped();

  /**
   * @brief Sets the intended state of the repeat effect.
   * Compares the intended state with the current state and performs necessary
   * actions (activate, deactivate, set length).
   * @param intended_length std::nullopt to turn off repeat, or a value for the
   * desired length.
   */
  void set_intended_repeat_state(std::optional<uint32_t> intended_length);

  /**
   * @brief Sets the active MIDI note number for a specific track.
   * This note is used by default when new steps are created or when drumpads
   * are triggered.
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

  void record_velocity_hit(uint8_t track_index);
  void clear_velocity_hit(uint8_t track_index);
  [[nodiscard]] bool has_recent_velocity_hit(uint8_t track_index) const;

  /**
   * @brief Get the current retrigger mode for a track.
   * @param track_index The track index to check.
   * @return 0 for off, 1 for single retrigger, 2 for double retrigger.
   */
  [[nodiscard]] uint8_t get_retrigger_mode_for_track(uint8_t track_index) const;

  /**
   * @brief Get a reference to the active sequencer instance.
   */
  [[nodiscard]] musin::timing::Sequencer<NumTracks, NumSteps> &get_sequencer() {
    return sequencer_.get();
  }
  /**
   * @brief Get a const reference to the active sequencer instance.
   */
  [[nodiscard]] const musin::timing::Sequencer<NumTracks, NumSteps> &
  get_sequencer() const {
    return sequencer_.get();
  }

private:
  [[nodiscard]] size_t calculate_base_step_index() const;
  void process_track_step(size_t track_idx, size_t step_index_to_play);

  void initialize_active_notes();
  void initialize_all_sequencers();
  void initialize_timing_and_random();

  musin::timing::Sequencer<NumTracks, NumSteps> main_sequencer_;
  musin::timing::Sequencer<NumTracks, NumSteps> random_sequencer_;
  std::reference_wrapper<musin::timing::Sequencer<NumTracks, NumSteps>>
      sequencer_;
  std::atomic<uint32_t> current_step_counter;
  etl::array<std::optional<uint8_t>, NumTracks> last_played_note_per_track;
  etl::array<std::optional<size_t>, NumTracks> _just_played_step_per_track;
  musin::timing::TempoHandler &tempo_source;
  bool _running = false;
  std::atomic<bool> _step_is_due = false;
  std::atomic<uint8_t> _retrigger_due_mask{0};
  uint8_t last_phase_24_{0};

  bool repeat_active_ = false;
  uint32_t repeat_length_ = 0;
  uint32_t repeat_activation_step_index_ = 0;
  uint64_t repeat_activation_step_counter_ = 0;

  bool continuous_randomization_active_ = false;
  SequencerEffectRandom<NumTracks, NumSteps> random_effect_;
  SequencerEffectSwing swing_effect_;

  // Random offset mode state
  bool random_offset_mode_active_ = false;
  float current_randomness_level_ = 0.0f;
  etl::array<etl::array<size_t, 3>, NumTracks> random_offsets_per_track_;
  etl::array<size_t, NumTracks> current_offset_index_per_track_{};
  uint32_t offset_generation_counter_ = 0;
  RandomnessProvider randomness_provider_;
  etl::array<uint8_t, NumTracks> _active_note_per_track{};
  etl::array<bool, NumTracks> _pad_pressed_state{};
  etl::array<RetriggerMode, NumTracks> _retrigger_mode_per_track{};
  etl::array<bool, NumTracks> _has_active_velocity_hit{};

  // Persistence management (optional until filesystem is ready)
  std::optional<SequencerStorage<NumTracks, NumSteps>> storage_;

  // Logger reference
  musin::Logger &logger_;

  std::atomic<bool> swing_enabled_update_pending_{false};
  std::atomic<bool> pending_swing_enabled_{false};
  std::atomic<bool> swing_target_update_pending_{false};
  std::atomic<bool> pending_swing_target_delays_odd_{false};

  void create_persistent_state(SequencerPersistentState &state) const;
  void apply_persistent_state(const SequencerPersistentState &state);

  etl::array<bool, NumTracks> &_pad_pressed_state_for_testing() {
    return _pad_pressed_state;
  }

public:
  void activate_repeat(uint32_t length);
  void deactivate_repeat();
  /**
   * @brief Activates retriggering for a specific track.
   * @param track_index The track to activate retriggering on.
   * @param mode 1 for single retrigger per step, 2 for double retrigger per
   * step.
   */
  void activate_play_on_every_step(uint8_t track_index, uint8_t mode);
  void activate_play_on_every_step(uint8_t track_index, RetriggerMode mode);
  /**
   * @brief Deactivates retriggering for a specific track.
   * @param track_index The track to deactivate retriggering on.
   */
  void deactivate_play_on_every_step(uint8_t track_index);
  void set_repeat_length(uint32_t length);
  [[nodiscard]] bool is_repeat_active() const;
  [[nodiscard]] uint32_t get_repeat_length() const;

  /**
   * @brief Copy the main pattern to the random pattern.
   */
  void copy_to_random();

  /**
   * @brief Set the main sequencer as active.
   */
  void set_main_active();

  /**
   * @brief Set the random sequencer as active.
   */
  void select_random_sequencer();

  /**
   * @brief Save the current sequencer state to persistent storage.
   * @return true if save was successful, false otherwise
   */
  bool save_state_to_flash();

  /**
   * @brief Load sequencer state from persistent storage.
   * @return true if load was successful, false otherwise
   */
  bool load_state_from_flash();

  /**
   * @brief Initialize persistence subsystem after filesystem is ready.
   * Must be called after filesystem.init() succeeds.
   * @return true if initialization and state loading succeeded, false otherwise
   */
  bool init_persistence();

  /**
   * @brief Check if persistence subsystem is initialized.
   * @return true if persistence is available, false otherwise
   */
  [[nodiscard]] bool is_persistence_initialized() const;

  /**
   * @brief Manually mark sequencer state as dirty for persistence.
   * Call this after modifying sequencer patterns via get_sequencer().
   */
  void mark_state_dirty_public();
};

} // namespace drum

#endif // DRUM_SEQUENCER_CONTROLLER_H

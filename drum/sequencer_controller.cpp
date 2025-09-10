#include "sequencer_controller.h"
#include "config.h"
#include "events.h"
#include "pico/time.h"
#include "pizza_controls.h" // For config constants
#include "sequencer_persistence.h"
#include "sequencer_storage.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace drum {

namespace musical_timing {
constexpr uint8_t PPQN = 24;
constexpr uint8_t DOWNBEAT = 0;
constexpr uint8_t STRAIGHT_OFFBEAT = PPQN / 2;      // 12
constexpr uint8_t TRIPLET_SUBDIVISION = PPQN / 3;   // 8
constexpr uint8_t SIXTEENTH_SUBDIVISION = PPQN / 4; // 6

constexpr uint8_t swing_offbeat_phase() {
  return static_cast<uint8_t>(drum::config::timing::DEFAULT_SWING_PRESET);
}
} // namespace musical_timing

template <size_t NumTracks, size_t NumSteps>
SequencerController<NumTracks, NumSteps>::SequencerController(
    musin::timing::TempoHandler &tempo_handler_ref, musin::Logger &logger)
    : sequencer_(main_sequencer_), current_step_counter{0},
      last_played_note_per_track{}, _just_played_step_per_track{},
      tempo_source(tempo_handler_ref), _running(false), _step_is_due{false},
      continuous_randomization_active_(false), _active_note_per_track{},
      _pad_pressed_state{}, _retrigger_mode_per_track{}, logger_(logger) {

  initialize_active_notes();
  initialize_all_sequencers();
  initialize_timing_and_random();

  // Note: Persistence initialization deferred until filesystem is ready
  // Call init_persistence() after filesystem.init() succeeds
}

template <size_t NumTracks, size_t NumSteps>
SequencerController<NumTracks, NumSteps>::~SequencerController() {
  if (_running) {
    tempo_source.remove_observer(*this);
  }
}

template <size_t NumTracks, size_t NumSteps>
size_t
SequencerController<NumTracks, NumSteps>::calculate_base_step_index() const {
  const size_t num_steps = sequencer_.get().get_num_steps();
  if (num_steps == 0)
    return 0;

  if (repeat_active_ && repeat_length_ > 0) {
    uint64_t steps_since_activation =
        current_step_counter - repeat_activation_step_counter_;
    uint64_t loop_position = steps_since_activation % repeat_length_;
    return (repeat_activation_step_index_ + loop_position) % num_steps;
  } else {
    return current_step_counter % num_steps;
  }
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::process_track_step(
    size_t track_idx, size_t step_index_to_play) {
  const size_t num_steps = sequencer_.get().get_num_steps();
  uint8_t track_index_u8 = static_cast<uint8_t>(track_idx);

  // Emit Note Off event if a note was previously playing on this track
  if (last_played_note_per_track[track_idx].has_value()) {
    drum::Events::NoteEvent note_off_event{
        .track_index = track_index_u8,
        .note = last_played_note_per_track[track_idx].value(),
        .velocity = 0};
    this->notify_observers(note_off_event);
    last_played_note_per_track[track_idx] = std::nullopt;
  }

  const int effective_step_with_fixed_offset =
      static_cast<int>(step_index_to_play);
  const size_t wrapped_step =
      (num_steps > 0)
          ? ((effective_step_with_fixed_offset % static_cast<int>(num_steps) +
              num_steps) %
             num_steps)
          : 0;

  const musin::timing::Step &step =
      sequencer_.get().get_track(track_idx).get_step(wrapped_step);
  bool actually_enabled = step.enabled;

  if (actually_enabled && step.note.has_value() && step.velocity.has_value() &&
      step.velocity.value() > 0) {
    drum::Events::NoteEvent note_on_event{.track_index = track_index_u8,
                                          .note = step.note.value(),
                                          .velocity = step.velocity.value()};
    this->notify_observers(note_on_event);
    last_played_note_per_track[track_idx] = step.note.value();
  }
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::set_swing_enabled(bool enabled) {
  swing_enabled_ = enabled;
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::set_swing_target(
    bool delay_odd) {
  if (swing_delays_odd_steps_ != delay_odd) {
    swing_delays_odd_steps_ = delay_odd;
  }
}

template <size_t NumTracks, size_t NumSteps>
bool SequencerController<NumTracks, NumSteps>::is_swing_enabled() const {
  return swing_enabled_;
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::reset() {
  for (size_t track_idx = 0; track_idx < last_played_note_per_track.size();
       ++track_idx) {
    if (last_played_note_per_track[track_idx].has_value()) {
      drum::Events::NoteEvent note_off_event{
          .track_index = static_cast<uint8_t>(track_idx),
          .note = last_played_note_per_track[track_idx].value(),
          .velocity = 0};
      this->notify_observers(note_off_event);
      last_played_note_per_track[track_idx] = std::nullopt;
    }
  }
  current_step_counter = 0;

  _just_played_step_per_track.fill(std::nullopt);

  // Pre-populate last-played step indices so the UI has a cursor immediately
  // after starting, even before the first incoming tick.
  size_t base_step_index = calculate_base_step_index();
  size_t num_tracks = sequencer_.get().get_num_tracks();
  for (size_t track_idx = 0; track_idx < num_tracks; ++track_idx) {
    _just_played_step_per_track[track_idx] = base_step_index;
  }

  deactivate_repeat();
  stop_continuous_randomization();
  for (size_t i = 0; i < NumTracks; ++i) {
    deactivate_play_on_every_step(static_cast<uint8_t>(i));
  }
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::advance_step() {
  _step_is_due = true;
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::start() {
  if (_running) {
    return;
  }

  _just_played_step_per_track.fill(std::nullopt);

  // Reset timing base so we don't wait for an old target accumulated
  // during manual advances while stopped.
  high_res_tick_counter_.store(0);
  next_trigger_tick_target_.store(0);

  tempo_source.add_observer(*this);
  tempo_source.set_playback_state(musin::timing::PlaybackState::PLAYING);

  _running = true;

  // Trigger the first step immediately upon start
  advance_step();
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::stop() {
  if (!_running) {
    return;
  }
  tempo_source.set_playback_state(musin::timing::PlaybackState::STOPPED);
  tempo_source.remove_observer(*this);
  _running = false;

  for (size_t track_idx = 0; track_idx < last_played_note_per_track.size();
       ++track_idx) {
    if (last_played_note_per_track[track_idx].has_value()) {
      drum::Events::NoteEvent note_off_event{
          .track_index = static_cast<uint8_t>(track_idx),
          .note = last_played_note_per_track[track_idx].value(),
          .velocity = 0};
      this->notify_observers(note_off_event);
    }
  }
  for (size_t i = 0; i < NumTracks; ++i) {
    deactivate_play_on_every_step(static_cast<uint8_t>(i));
  }
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::notification(
    musin::timing::TempoEvent event) {
  if (!_running) {
    return;
  }

  // event.phase_24 is guaranteed in [0, PPQN-1] by TempoHandler

  // Handle resync events by immediately advancing a step
  if (event.is_resync) {
    advance_step();
    return;
  }

  // Simple anchors per mode; no recompute or downbeat gating
  bool on_anchor = false;
  if (!swing_enabled_) {
    on_anchor = (event.phase_24 == musical_timing::DOWNBEAT) ||
                (event.phase_24 == musical_timing::STRAIGHT_OFFBEAT);
  } else if (swing_delays_odd_steps_) {
    on_anchor =
        (event.phase_24 == musical_timing::DOWNBEAT) || (event.phase_24 == 16);
  } else {
    on_anchor = (event.phase_24 == 4) ||
                (event.phase_24 == musical_timing::STRAIGHT_OFFBEAT);
  }

  if (on_anchor) {
    _step_is_due = true;
  }

  // Process retrigger logic based on phases
  // Swing OFF behavior:
  //   - Mode 1: eighths (0, 12)
  //   - Mode 2: sixteenths (0, 6, 12, 18)
  // Swing ON behavior:
  //   - Mode 1: follows swing grid (0 and DEFAULT_SWING_PRESET)
  //   - Mode 2: triplets (every 8 phases: 0, 8, 16)
  for (size_t track_idx = 0; track_idx < NumTracks; ++track_idx) {
    uint8_t mode = _retrigger_mode_per_track[track_idx];
    if (mode == 0) {
      continue;
    }

    bool due = false;
    if (mode == 1) {
      // Trigger on the same anchors as main step timing
      if (!swing_enabled_) {
        due = (event.phase_24 == musical_timing::DOWNBEAT) ||
              (event.phase_24 == musical_timing::STRAIGHT_OFFBEAT);
      } else if (swing_delays_odd_steps_) {
        due = (event.phase_24 == musical_timing::DOWNBEAT) ||
              (event.phase_24 == 16);
      } else {
        due = (event.phase_24 == 4) ||
              (event.phase_24 == musical_timing::STRAIGHT_OFFBEAT);
      }
    } else if (mode == 2) {
      // Preserve existing contrast: triplets when swing ON; sixteenths when OFF
      if (swing_enabled_) {
        due = (event.phase_24 % musical_timing::TRIPLET_SUBDIVISION) == 0;
      } else {
        due = (event.phase_24 % musical_timing::SIXTEENTH_SUBDIVISION) == 0;
      }
    }

    if (due) {
      _retrigger_due_mask |= (1 << track_idx);
    }
  }
}

template <size_t NumTracks, size_t NumSteps>
[[nodiscard]] uint32_t
SequencerController<NumTracks, NumSteps>::get_current_step() const noexcept {
  const size_t num_steps = sequencer_.get().get_num_steps();
  if (num_steps == 0)
    return 0;
  return current_step_counter % num_steps;
}

template <size_t NumTracks, size_t NumSteps>
[[nodiscard]] std::optional<size_t>
SequencerController<NumTracks, NumSteps>::get_last_played_step_for_track(
    size_t track_idx) const {
  if ((track_idx < NumTracks)) {
    return _just_played_step_per_track[track_idx];
  }
  return std::nullopt;
}

template <size_t NumTracks, size_t NumSteps>
[[nodiscard]] bool
SequencerController<NumTracks, NumSteps>::is_running() const {
  return _running;
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::activate_repeat(
    uint32_t length) {
  if (_running && !repeat_active_) {
    repeat_active_ = true;
    repeat_length_ = std::max(uint32_t{1}, length);
    const size_t num_steps = sequencer_.get().get_num_steps();
    repeat_activation_step_index_ =
        (num_steps > 0) ? (current_step_counter % num_steps) : 0;
    repeat_activation_step_counter_ = current_step_counter;
  }
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::deactivate_repeat() {
  if (repeat_active_) {
    repeat_active_ = false;
    repeat_length_ = 0;
  }
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::set_repeat_length(
    uint32_t length) {
  if (repeat_active_) {
    uint32_t new_length = std::max(uint32_t{1}, length);
    if (new_length != repeat_length_) {
      repeat_length_ = new_length;
    }
  }
}

template <size_t NumTracks, size_t NumSteps>
[[nodiscard]] bool
SequencerController<NumTracks, NumSteps>::is_repeat_active() const {
  return repeat_active_;
}

template <size_t NumTracks, size_t NumSteps>
[[nodiscard]] uint32_t
SequencerController<NumTracks, NumSteps>::get_repeat_length() const {
  return repeat_active_ ? repeat_length_ : 0;
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::generate_random_pattern() {
  // Generate random pattern: copy notes from main, randomize velocities and
  // enable states
  for (size_t track_idx = 0; track_idx < NumTracks; ++track_idx) {
    auto &main_track = main_sequencer_.get_track(track_idx);
    auto &random_track = random_sequencer_.get_track(track_idx);

    for (size_t step_idx = 0; step_idx < NumSteps; ++step_idx) {
      auto &main_step = main_track.get_step(step_idx);
      auto &random_step = random_track.get_step(step_idx);

      // Copy note from main sequencer
      random_step.note = main_step.note;

      // Get one random value and extract both velocity and enabled from it
      uint32_t random_value = rand();

      // Extract velocity from lower 7 bits (0-127) for full MIDI range
      random_step.velocity = random_value & 0x7F;

      // Extract enabled from bit 6 (50% chance)
      random_step.enabled = (random_value & 0x40) != 0;
    }
  }
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks,
                         NumSteps>::start_continuous_randomization() {
  continuous_randomization_active_ = true;
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::stop_continuous_randomization() {
  continuous_randomization_active_ = false;
}

template <size_t NumTracks, size_t NumSteps>
[[nodiscard]] bool
SequencerController<NumTracks, NumSteps>::is_continuous_randomization_active()
    const {
  return continuous_randomization_active_;
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::set_random(float value) {
  value = std::clamp(value, 0.0f, 1.0f);

  bool was_using_random = (&sequencer_.get() == &random_sequencer_);

  // Use main sequencer for low values
  if (value < 0.2f) {
    set_main_active();
    stop_continuous_randomization();
    return;
  }

  // Switch to random sequencer and generate new pattern every time we cross
  // 0.2f threshold
  if (!was_using_random) {
    // Crossing from main to random - switch sequencer and generate fresh
    // pattern
    select_random_sequencer();
    generate_random_pattern();
  }

  // Control continuous randomization separately for high values
  if (value > 0.8f) {
    start_continuous_randomization();
  } else {
    stop_continuous_randomization();
  }
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::set_intended_repeat_state(
    std::optional<uint32_t> intended_length) {
  bool should_be_active = intended_length.has_value();
  bool was_active = is_repeat_active();

  if (should_be_active && !was_active) {
    activate_repeat(intended_length.value());
  } else if (!should_be_active && was_active) {
    deactivate_repeat();
  } else if (should_be_active && was_active) {
    set_repeat_length(intended_length.value());
  }
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::toggle() {
  if (is_running()) {
    stop();
  } else {
    start();
  }
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::notification(
    drum::Events::SysExTransferStateChangeEvent event) {
  if (event.is_active) {
    stop();
  }
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::trigger_note_on(
    uint8_t track_index, uint8_t note, uint8_t velocity) {
  // Debug: Log that trigger_note_on was called
  static_cast<void>(0); // Placeholder for debug log - will add proper logging

  // Ensure any previously playing note on this track is turned off first
  if (last_played_note_per_track[track_index].has_value()) {
    if (last_played_note_per_track[track_index].value() !=
        note) { // Only send note off if it's a different note
      drum::Events::NoteEvent note_off_event{
          .track_index = track_index,
          .note = last_played_note_per_track[track_index].value(),
          .velocity = 0};
      this->notify_observers(note_off_event);
    }
  }

  drum::Events::NoteEvent note_on_event{
      .track_index = track_index, .note = note, .velocity = velocity};
  this->notify_observers(note_on_event);
  last_played_note_per_track[track_index] = note;
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::trigger_note_off(
    uint8_t track_index, uint8_t note) {
  // Only send note off if this note was the one playing
  if (last_played_note_per_track[track_index].has_value() &&
      last_played_note_per_track[track_index].value() == note) {
    drum::Events::NoteEvent note_off_event{
        .track_index = track_index, .note = note, .velocity = 0};
    this->notify_observers(note_off_event);
    last_played_note_per_track[track_index] = std::nullopt;
  }
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::set_active_note_for_track(
    uint8_t track_index, uint8_t note) {
  if (track_index < NumTracks) {
    _active_note_per_track[track_index] = note;
    if (storage_.has_value()) {
      storage_->mark_state_dirty();
    }
  }
  // else: track_index is out of bounds, do nothing or log an error.
  // For now, we silently ignore out-of-bounds access to prevent crashes.
}

template <size_t NumTracks, size_t NumSteps>
uint8_t SequencerController<NumTracks, NumSteps>::get_active_note_for_track(
    uint8_t track_index) const {
  if (track_index < NumTracks) {
    return _active_note_per_track[track_index];
  }
  // track_index is out of bounds. Return a default/safe value.
  // 0 is a common default for MIDI notes, though often unassigned.
  return 0;
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::set_pad_pressed_state(
    uint8_t track_index, bool is_pressed) {
  if (track_index < NumTracks) {
    _pad_pressed_state[track_index] = is_pressed;
  }
}

template <size_t NumTracks, size_t NumSteps>
bool SequencerController<NumTracks, NumSteps>::is_pad_pressed(
    uint8_t track_index) const {
  if (track_index < NumTracks) {
    return _pad_pressed_state[track_index];
  }
  return false;
}

template <size_t NumTracks, size_t NumSteps>
uint8_t SequencerController<NumTracks, NumSteps>::get_retrigger_mode_for_track(
    uint8_t track_index) const {
  if (track_index < NumTracks) {
    return _retrigger_mode_per_track[track_index];
  }
  return 0;
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::activate_play_on_every_step(
    uint8_t track_index, uint8_t mode) {
  if (track_index < NumTracks && (mode == 1 || mode == 2)) {
    _retrigger_mode_per_track[track_index] = mode;
  }
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::deactivate_play_on_every_step(
    uint8_t track_index) {
  if (track_index < NumTracks) {
    _retrigger_mode_per_track[track_index] = 0;
  }
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::update() {
  // Periodic save logic with debouncing (runs regardless of step timing)
  if (storage_.has_value() && storage_->should_save_now()) {
    SequencerPersistentState state;
    create_persistent_state(state);
    if (storage_->save_state_to_flash(state)) {
      logger_.debug("Periodic save completed successfully");
    } else {
      logger_.warn("Periodic save failed");
    }
  }

  uint8_t current_retrigger_mask = _retrigger_due_mask.load();
  if (current_retrigger_mask > 0) {
    uint8_t processed_mask = 0;
    for (size_t track_idx = 0; track_idx < NumTracks; ++track_idx) {
      if ((current_retrigger_mask >> track_idx) & 1) {
        uint8_t note_to_play =
            get_active_note_for_track(static_cast<uint8_t>(track_idx));
        trigger_note_on(static_cast<uint8_t>(track_idx), note_to_play,
                        drum::config::drumpad::RETRIGGER_VELOCITY);
        processed_mask |= (1 << track_idx);
      }
    }
    _retrigger_due_mask.fetch_and(~processed_mask);
  }

  if (!_step_is_due) {
    return;
  }
  _step_is_due = false;

  _just_played_step_per_track.fill(std::nullopt);

  size_t base_step_index = calculate_base_step_index();

  size_t num_tracks = sequencer_.get().get_num_tracks();

  for (size_t track_idx = 0; track_idx < num_tracks; ++track_idx) {
    size_t step_index_to_play_for_track = base_step_index;

    _just_played_step_per_track[track_idx] = step_index_to_play_for_track;
    process_track_step(track_idx, step_index_to_play_for_track);

    // Handle the first retrigger note for the main step event
    if (_retrigger_mode_per_track[track_idx] > 0) {
      uint8_t note_to_play =
          get_active_note_for_track(static_cast<uint8_t>(track_idx));
      trigger_note_on(static_cast<uint8_t>(track_idx), note_to_play,
                      drum::config::drumpad::RETRIGGER_VELOCITY);
    }
  }

  // --- Random per-track-ahead logic ---
  if (continuous_randomization_active_ && !repeat_active_) {
    const size_t num_steps = sequencer_.get().get_num_steps();
    if (num_steps > 0) {
      // Randomize each track at its own offset ahead
      for (size_t track_idx = 0; track_idx < num_tracks; ++track_idx) {
        size_t track_offset =
            RANDOM_STEP_OFFSETS[track_idx % RANDOM_STEP_OFFSETS.size()];
        size_t steps_ahead_index =
            (current_step_counter + track_offset) % num_steps;

        auto &random_track = random_sequencer_.get_track(track_idx);
        auto &random_step = random_track.get_step(steps_ahead_index);

        // Copy note from main sequencer if available
        const auto &main_step =
            main_sequencer_.get_track(track_idx).get_step(steps_ahead_index);
        random_step.note = main_step.note;

        // Generate random velocity and enable state
        uint32_t random_value = rand();
        random_step.velocity = random_value & 0x7F;       // 0-127
        random_step.enabled = (random_value & 0x40) != 0; // 50% chance
      }
    }
  }

  current_step_counter++;
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::copy_to_random() {
  random_sequencer_ = main_sequencer_;
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::set_main_active() {
  sequencer_ = std::ref(main_sequencer_);
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::select_random_sequencer() {
  sequencer_ = std::ref(random_sequencer_);
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::initialize_active_notes() {
  for (size_t track_idx = 0; track_idx < NumTracks; ++track_idx) {
    if (track_idx < config::track_note_ranges.size() &&
        !config::track_note_ranges[track_idx].empty()) {
      _active_note_per_track[track_idx] =
          config::track_note_ranges[track_idx][0];
    }
  }
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::initialize_all_sequencers() {
  for (size_t track_idx = 0; track_idx < NumTracks; ++track_idx) {
    uint8_t initial_note = _active_note_per_track[track_idx];
    auto &main_track = main_sequencer_.get_track(track_idx);
    auto &random_track = random_sequencer_.get_track(track_idx);
    for (size_t step_idx = 0; step_idx < NumSteps; ++step_idx) {
      auto &main_step = main_track.get_step(step_idx);
      auto &random_step = random_track.get_step(step_idx);
      main_step.note = initial_note;
      main_step.velocity = drum::config::keypad::DEFAULT_STEP_VELOCITY;
      random_step.note = initial_note;
      random_step.velocity = drum::config::keypad::DEFAULT_STEP_VELOCITY;
    }
  }
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::initialize_timing_and_random() {
  srand(time_us_32());
  _just_played_step_per_track.fill(std::nullopt);
  _pad_pressed_state.fill(false);
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::create_persistent_state(
    SequencerPersistentState &state) const {
  // Clear the state and set header
  state = SequencerPersistentState();

  // Copy track data from main sequencer only
  for (size_t track_idx = 0;
       track_idx < NumTracks && track_idx < config::NUM_TRACKS; ++track_idx) {
    const auto &track = main_sequencer_.get_track(track_idx);
    for (size_t step_idx = 0;
         step_idx < NumSteps && step_idx < config::NUM_STEPS_PER_TRACK;
         ++step_idx) {
      const auto &step = track.get_step(step_idx);
      // Persist only velocity; 0 velocity means disabled.
      if (step.enabled && step.velocity.has_value() &&
          step.velocity.value() > 0) {
        state.tracks[track_idx].velocities[step_idx] = step.velocity.value();
      } else {
        state.tracks[track_idx].velocities[step_idx] = 0;
      }
    }
  }

  // Copy active notes per track
  for (size_t track_idx = 0;
       track_idx < NumTracks && track_idx < config::NUM_TRACKS; ++track_idx) {
    state.active_notes[track_idx] = _active_note_per_track[track_idx];
  }
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::apply_persistent_state(
    const SequencerPersistentState &state) {
  // Apply active notes first
  for (size_t track_idx = 0;
       track_idx < NumTracks && track_idx < config::NUM_TRACKS; ++track_idx) {
    _active_note_per_track[track_idx] = state.active_notes[track_idx];
  }

  // Apply per-step velocities to main sequencer; note is derived from active
  // note
  for (size_t track_idx = 0;
       track_idx < NumTracks && track_idx < config::NUM_TRACKS; ++track_idx) {
    auto &track = main_sequencer_.get_track(track_idx);
    uint8_t track_note = _active_note_per_track[track_idx];
    // Ensure track default note matches active note
    track.set_note(track_note);
    for (size_t step_idx = 0;
         step_idx < NumSteps && step_idx < config::NUM_STEPS_PER_TRACK;
         ++step_idx) {
      auto &step = track.get_step(step_idx);
      uint8_t velocity = state.tracks[track_idx].velocities[step_idx];
      if (velocity > 0) {
        step.note = track_note;
        step.velocity = velocity;
        step.enabled = true;
      } else {
        step.note = std::nullopt;
        step.velocity = std::nullopt;
        step.enabled = false;
      }
    }
  }
}

template <size_t NumTracks, size_t NumSteps>
bool SequencerController<NumTracks, NumSteps>::save_state_to_flash() {
  if (!storage_.has_value()) {
    logger_.error("Manual save to flash failed - persistence not initialized");
    return false;
  }

  SequencerPersistentState state;
  create_persistent_state(state);
  bool success = storage_->save_state_to_flash(state);
  if (success) {
    logger_.info("Manual save to flash completed successfully");
  } else {
    logger_.error("Manual save to flash failed");
  }
  return success;
}

template <size_t NumTracks, size_t NumSteps>
bool SequencerController<NumTracks, NumSteps>::load_state_from_flash() {
  if (!storage_.has_value()) {
    logger_.error(
        "Manual load from flash failed - persistence not initialized");
    return false;
  }

  SequencerPersistentState state;
  if (storage_->load_state_from_flash(state)) {
    apply_persistent_state(state);
    logger_.info("Manual load from flash completed successfully");
    return true;
  }
  logger_.warn("Manual load from flash failed - no valid state found");
  return false;
}

template <size_t NumTracks, size_t NumSteps>
bool SequencerController<NumTracks, NumSteps>::init_persistence() {
  if (storage_.has_value()) {
    logger_.warn("Persistence already initialized");
    return true;
  }

  // Initialize storage now that filesystem is ready
  storage_.emplace();

  // Attempt to load existing state
  SequencerPersistentState loaded_state;
  if (storage_->load_state_from_flash(loaded_state)) {
    apply_persistent_state(loaded_state);
    logger_.info("Sequencer state loaded from flash during init_persistence");
    return true;
  } else {
    logger_.info(
        "No sequencer state found during init_persistence, using defaults");
    return false;
  }
}

template <size_t NumTracks, size_t NumSteps>
bool SequencerController<NumTracks, NumSteps>::is_persistence_initialized()
    const {
  return storage_.has_value();
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::mark_state_dirty_public() {
  if (storage_.has_value()) {
    storage_->mark_state_dirty();
  }
}

// Explicit template instantiation for 4 tracks, 8 steps
template class SequencerController<4, 8>;

} // namespace drum

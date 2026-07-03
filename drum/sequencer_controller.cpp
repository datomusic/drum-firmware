#include "sequencer_controller.h"
#include "config.h"
#include "events.h"
#include "pico/time.h"
#include "sequencer_persistence.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace drum {

template <size_t NumTracks, size_t NumSteps>
SequencerController<NumTracks, NumSteps>::SequencerController(
    musin::timing::TempoHandler &tempo_handler_ref, musin::Logger &logger)
    : sequencer_(main_sequencer_), scheduled_step_counter_{0},
      tempo_source(tempo_handler_ref), _running(false), _step_is_due{false},
      persistence_(logger), logger_(logger) {

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
  return repeat_effect_.calculate_step_index(scheduled_step_counter_,
                                             sequencer_.get().get_num_steps());
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::process_track_step(
    size_t track_idx, size_t step_index_to_play) {
  const size_t num_steps = sequencer_.get().get_num_steps();
  uint8_t track_index_u8 = static_cast<uint8_t>(track_idx);
  TrackState &track_state = track_states_[track_idx];

  // Emit Note Off event if a note was previously playing on this track
  if (track_state.last_played_note.has_value()) {
    drum::Events::NoteEvent note_off_event{
        .track_index = track_index_u8,
        .note = track_state.last_played_note.value(),
        .velocity = 0};
    this->notify_observers(note_off_event);
    track_state.last_played_note = std::nullopt;
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
  uint8_t effective_velocity = step.velocity.value_or(0);

  // Apply probability flip for hard press random mode
  if (random_effect_.is_probability_mode_enabled()) {
    // 50% chance of flipping any step's enabled state using lowest bit
    if (rand() & 1) {
      actually_enabled = !actually_enabled;
      // If flipping a disabled step to enabled, ensure it has velocity
      if (actually_enabled && effective_velocity == 0) {
        effective_velocity = drum::config::keypad::DEFAULT_STEP_VELOCITY;
      }
    }
  }

  if (actually_enabled && step.note.has_value() && effective_velocity > 0) {
    drum::Events::NoteEvent note_on_event{.track_index = track_index_u8,
                                          .note = step.note.value(),
                                          .velocity = effective_velocity};
    this->notify_observers(note_on_event);
    track_state.last_played_note = step.note.value();
  }
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::set_swing_enabled(bool enabled) {
  pending_swing_enabled_ = enabled;
  swing_enabled_update_pending_ = true;
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::set_swing_target(
    bool delay_odd) {
  pending_swing_target_delays_odd_ = delay_odd;
  swing_target_update_pending_ = true;
}

template <size_t NumTracks, size_t NumSteps>
bool SequencerController<NumTracks, NumSteps>::is_swing_enabled() const {
  return swing_effect_.is_swing_enabled();
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::reset() {
  for (size_t track_idx = 0; track_idx < NumTracks; ++track_idx) {
    TrackState &track_state = track_states_[track_idx];
    if (track_state.last_played_note.has_value()) {
      drum::Events::NoteEvent note_off_event{
          .track_index = static_cast<uint8_t>(track_idx),
          .note = track_state.last_played_note.value(),
          .velocity = 0};
      this->notify_observers(note_off_event);
      track_state.last_played_note = std::nullopt;
    }
  }
  scheduled_step_counter_ = 0;
  last_phase_12_ = 0;

  // Pre-populate last-played step indices so the UI has a cursor immediately
  // after starting, even before the first incoming tick.
  size_t base_step_index = calculate_base_step_index();
  for (auto &track_state : track_states_) {
    track_state.just_played_step = base_step_index;
  }

  deactivate_repeat();
  disable_random_offset_mode();
  disable_random_probability_mode();
  clear_traces();
  for (size_t i = 0; i < NumTracks; ++i) {
    deactivate_play_on_every_step(static_cast<uint8_t>(i));
  }
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::mark_step_due() {
  _step_is_due = true;
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::start() {
  if (_running) {
    return;
  }

  for (auto &track_state : track_states_) {
    track_state.just_played_step = std::nullopt;
  }

  tempo_source.add_observer(*this);
  tempo_source.set_playback_state(musin::timing::PlaybackState::PLAYING);

  _running = true;

  // To maintain swing timing, align the clock phase on restart.
  const auto [anchor_phase, primed_phase] =
      align_to_last_anchor(last_phase_12_);
  last_phase_12_ = primed_phase;
  if (drum::config::RETRIGGER_SYNC_ON_PLAYBUTTON) {
    tempo_source.trigger_manual_sync(anchor_phase);
  }
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::stop() {
  if (!_running) {
    return;
  }
  tempo_source.set_playback_state(musin::timing::PlaybackState::STOPPED);
  tempo_source.remove_observer(*this);
  _running = false;

  for (size_t track_idx = 0; track_idx < NumTracks; ++track_idx) {
    if (track_states_[track_idx].last_played_note.has_value()) {
      drum::Events::NoteEvent note_off_event{
          .track_index = static_cast<uint8_t>(track_idx),
          .note = track_states_[track_idx].last_played_note.value(),
          .velocity = 0};
      this->notify_observers(note_off_event);
    }
  }
  for (size_t i = 0; i < NumTracks; ++i) {
    deactivate_play_on_every_step(static_cast<uint8_t>(i));
  }

  random_effect_.reset_to_inactive();
  random_intends_flip_ = false;
  clear_traces();
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::notification(
    musin::timing::TempoEvent event) {
  if (!_running) {
    return;
  }

  // Apply pending swing changes on the downbeat to ensure timing stability
  if (event.phase_12 == 0) { // Downbeat
    if (swing_enabled_update_pending_.load(std::memory_order_relaxed)) {
      swing_effect_.set_swing_enabled(
          pending_swing_enabled_.load(std::memory_order_relaxed));
      swing_enabled_update_pending_.store(false, std::memory_order_relaxed);
    }
    if (swing_target_update_pending_.load(std::memory_order_relaxed)) {
      swing_effect_.set_swing_target(
          pending_swing_target_delays_odd_.load(std::memory_order_relaxed));
      swing_target_update_pending_.store(false, std::memory_order_relaxed);
    }
  }

  // event.phase_12 is guaranteed in [0, PPQN-1] by TempoHandler

  // Handle resync events by setting up the look-behind window
  // This makes the next expected phase catchable by normal scheduling
  if (event.is_resync) {
    last_phase_12_ = (event.phase_12 + musin::timing::DEFAULT_PPQN - 1) %
                     musin::timing::DEFAULT_PPQN;
    // Don't return - fall through to normal look-behind logic
  }

  // If no time has passed, do nothing.
  if (event.phase_12 == last_phase_12_) {
    return;
  }

  // Calculate swing timing using the dedicated effect
  const size_t next_index = calculate_base_step_index();
  const auto timing = swing_effect_.calculate_step_timing(
      next_index, repeat_effect_.is_active(), scheduled_step_counter_);
  const uint8_t expected_phase = timing.expected_phase;

  // --- Look-behind scheduling check for the main step ---
  // Check if the expected_phase falls within the window between the last tick
  // and the current tick.
  bool is_step_due = false;
  if (event.phase_12 < last_phase_12_) { // Phase wrapped around (e.g., 11 -> 0)
    is_step_due =
        (expected_phase > last_phase_12_) || (expected_phase <= event.phase_12);
  } else { // Phase did not wrap
    is_step_due =
        (expected_phase > last_phase_12_) && (expected_phase <= event.phase_12);
  }

  if (is_step_due) {
    mark_step_due();
  }

  // --- Look-behind scheduling for retrigger substeps ---
  const std::uint32_t substep_grid_mask = timing.substep_mask;

  // Create a bitmask for the phase range that has passed since the last tick.
  uint32_t range_mask = 0;
  if (event.phase_12 < last_phase_12_) { // Phase wrapped
    // Range: (last_phase_12_, 11]
    uint32_t mask1 = ((1u << (11 - last_phase_12_)) - 1)
                     << (last_phase_12_ + 1);
    // Range: [0, event.phase_12]
    uint32_t mask2 = (1u << (event.phase_12 + 1)) - 1;
    range_mask = mask1 | mask2;
  } else { // No wrap
    // Range: (last_phase_12_, event.phase_12]
    range_mask = ((1u << (event.phase_12 - last_phase_12_)) - 1)
                 << (last_phase_12_ + 1);
  }

  const bool substep_is_due = (substep_grid_mask & range_mask) != 0;

  retrigger_effect_.mark_due_tracks(is_step_due, substep_is_due);

  last_phase_12_ = event.phase_12;
}

template <size_t NumTracks, size_t NumSteps>
[[nodiscard]] uint32_t
SequencerController<NumTracks, NumSteps>::get_current_step() const noexcept {
  const size_t num_steps = sequencer_.get().get_num_steps();
  if (num_steps == 0)
    return 0;
  return scheduled_step_counter_ % num_steps;
}

template <size_t NumTracks, size_t NumSteps>
[[nodiscard]] std::optional<size_t>
SequencerController<NumTracks, NumSteps>::get_last_played_step_for_track(
    size_t track_idx) const {
  if ((track_idx < NumTracks)) {
    return track_states_[track_idx].just_played_step;
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
  if (_running) {
    repeat_effect_.activate(length, scheduled_step_counter_,
                            sequencer_.get().get_num_steps());
  }
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::deactivate_repeat() {
  repeat_effect_.deactivate();
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::set_repeat_length(
    uint32_t length) {
  repeat_effect_.set_length(length);
}

template <size_t NumTracks, size_t NumSteps>
[[nodiscard]] bool
SequencerController<NumTracks, NumSteps>::is_repeat_active() const {
  return repeat_effect_.is_active();
}

template <size_t NumTracks, size_t NumSteps>
[[nodiscard]] uint32_t
SequencerController<NumTracks, NumSteps>::get_repeat_length() const {
  return repeat_effect_.get_length();
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::set_random(float value) {
  random_effect_.set_random_intensity(value);
  if (value >= 0.2f) {
    random_effect_.regenerate_offsets(sequencer_.get().get_num_steps(),
                                      NumTracks);
  }
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::set_intended_repeat_state(
    std::optional<uint32_t> intended_length) {
  bool should_be_active = intended_length.has_value();
  bool was_active = is_repeat_active();

  if (should_be_active && !was_active) {
    activate_repeat(intended_length.value());
    // Enforce random-effect constraints under REPEAT
    random_effect_.request_state(random_effect_.get_current_state(),
                                 /*repeat_active=*/true);
  } else if (!should_be_active && was_active) {
    deactivate_repeat();
    // REPEAT released: if user intends flip and random is active, upgrade
    if (random_effect_.is_offset_mode_enabled() && random_intends_flip_) {
      random_effect_.request_state(RandomEffectState::OffsetWithFlip,
                                   /*repeat_active=*/false);
    }
  } else if (should_be_active && was_active) {
    set_repeat_length(intended_length.value());
    // Re-assert constraints in case state needs downgrading
    random_effect_.request_state(random_effect_.get_current_state(),
                                 /*repeat_active=*/true);
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
  TrackState &track_state = track_states_[track_index];
  if (track_state.last_played_note.has_value()) {
    if (track_state.last_played_note.value() !=
        note) { // Only send note off if it's a different note
      drum::Events::NoteEvent note_off_event{
          .track_index = track_index,
          .note = track_state.last_played_note.value(),
          .velocity = 0};
      this->notify_observers(note_off_event);
    }
  }

  drum::Events::NoteEvent note_on_event{
      .track_index = track_index, .note = note, .velocity = velocity};
  this->notify_observers(note_on_event);
  track_state.last_played_note = note;
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::trigger_note_off(
    uint8_t track_index, uint8_t note) {
  // Only send note off if this note was the one playing
  TrackState &track_state = track_states_[track_index];
  if (track_state.last_played_note.has_value() &&
      track_state.last_played_note.value() == note) {
    drum::Events::NoteEvent note_off_event{
        .track_index = track_index, .note = note, .velocity = 0};
    this->notify_observers(note_off_event);
    track_state.last_played_note = std::nullopt;
  }
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::set_active_note_for_track(
    uint8_t track_index, uint8_t note) {
  if (track_index < NumTracks) {
    track_states_[track_index].active_note = note;
    persistence_.mark_dirty();
  }
  // else: track_index is out of bounds, do nothing or log an error.
  // For now, we silently ignore out-of-bounds access to prevent crashes.
}

template <size_t NumTracks, size_t NumSteps>
uint8_t SequencerController<NumTracks, NumSteps>::get_active_note_for_track(
    uint8_t track_index) const {
  if (track_index < NumTracks) {
    return track_states_[track_index].active_note;
  }
  // track_index is out of bounds. Return a default/safe value.
  // 0 is a common default for MIDI notes, though often unassigned.
  return 0;
}

template <size_t NumTracks, size_t NumSteps>
uint8_t SequencerController<NumTracks, NumSteps>::get_retrigger_mode_for_track(
    uint8_t track_index) const {
  return static_cast<uint8_t>(retrigger_effect_.get_mode(track_index));
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::record_velocity_hit(
    uint8_t track_index) {
  if (track_index < NumTracks) {
    track_states_[track_index].has_velocity_hit = true;
    record_pad_hit_trace(track_index);
  }
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::record_pad_hit_trace(
    uint8_t track_index) {
  if (_running && track_index < NumTracks) {
    TrackState &track_state = track_states_[track_index];
    size_t trace_step =
        track_state.just_played_step.value_or(get_current_step());
    // A hit late in the step feels like it anticipates the next step, so the
    // trace is quantized forward; a hit clearly between two steps belongs to
    // neither and leaves no trace. Only the visualization is affected; the
    // sounded note keeps its live timing.
    switch (swing_effect_.classify_hit_phase(last_phase_12_)) {
    case SequencerEffectSwing::HitZone::Early:
      break;
    case SequencerEffectSwing::HitZone::Middle:
      return;
    case SequencerEffectSwing::HitZone::Late:
      trace_step = (trace_step + 1) % NumSteps;
      break;
    }
    trace_velocities_[track_index][trace_step] = TRACE_INITIAL_VELOCITY;
  }
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::fade_traces() {
  constexpr uint8_t DECREMENT =
      (TRACE_INITIAL_VELOCITY + TRACE_FADE_STEPS - 1) / TRACE_FADE_STEPS;
  for (auto &track_traces : trace_velocities_) {
    for (auto &velocity : track_traces) {
      velocity = (velocity > DECREMENT) ? (velocity - DECREMENT) : 0;
    }
  }
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::clear_traces() {
  for (auto &track_traces : trace_velocities_) {
    track_traces.fill(0);
  }
}

template <size_t NumTracks, size_t NumSteps>
uint8_t SequencerController<NumTracks, NumSteps>::get_trace_velocity(
    size_t track_idx, size_t step_idx) const {
  if (track_idx < NumTracks && step_idx < NumSteps) {
    return trace_velocities_[track_idx][step_idx];
  }
  return 0;
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::clear_velocity_hit(
    uint8_t track_index) {
  if (track_index < NumTracks) {
    track_states_[track_index].has_velocity_hit = false;
  }
}

template <size_t NumTracks, size_t NumSteps>
bool SequencerController<NumTracks, NumSteps>::has_recent_velocity_hit(
    uint8_t track_index) const {
  if (track_index < NumTracks) {
    return track_states_[track_index].has_velocity_hit;
  }
  return false;
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::activate_play_on_every_step(
    uint8_t track_index, RetriggerMode mode) {
  retrigger_effect_.set_mode(track_index, mode);
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::deactivate_play_on_every_step(
    uint8_t track_index) {
  retrigger_effect_.set_mode(track_index, RetriggerMode::Off);
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::update() {
  // Periodic save logic with debouncing (runs regardless of step timing)
  if (persistence_.should_save_now()) {
    SequencerPersistentState state;
    create_persistent_state(state);
    if (persistence_.save(state)) {
      logger_.debug("Periodic save completed successfully");
    } else {
      logger_.warn("Periodic save failed");
    }
  }

  uint8_t retrigger_mask = retrigger_effect_.take_due_mask();
  for (size_t track_idx = 0; track_idx < NumTracks; ++track_idx) {
    if ((retrigger_mask >> track_idx) & 1) {
      uint8_t note_to_play =
          get_active_note_for_track(static_cast<uint8_t>(track_idx));
      trigger_note_on(static_cast<uint8_t>(track_idx), note_to_play,
                      drum::config::drumpad::RETRIGGER_VELOCITY);
      record_pad_hit_trace(static_cast<uint8_t>(track_idx));
    }
  }

  if (!_step_is_due) {
    return;
  }
  _step_is_due = false;

  // Fade before this step's hits are recorded, so a fresh trace stays at
  // full brightness for the whole step window it landed on.
  fade_traces();

  size_t base_step_index = calculate_base_step_index();

  for (auto &track_state : track_states_) {
    track_state.just_played_step = std::nullopt;
  }

  size_t num_tracks = sequencer_.get().get_num_tracks();

  for (size_t track_idx = 0; track_idx < num_tracks; ++track_idx) {
    // Calculate randomized step using the effect
    const size_t num_steps = sequencer_.get().get_num_steps();
    auto randomized_step = random_effect_.calculate_randomized_step(
        base_step_index, track_idx, num_steps, repeat_effect_.is_active());

    size_t step_index_to_play_for_track = randomized_step.effective_step_index;

    track_states_[track_idx].just_played_step = step_index_to_play_for_track;
    process_track_step(track_idx, step_index_to_play_for_track);

    // Handle the first retrigger note for the main step event
    // Simple guard: only add explicit boundary retrigger for Step mode
    if (retrigger_effect_.get_mode(static_cast<uint8_t>(track_idx)) ==
        RetriggerMode::Step) {
      uint8_t note_to_play =
          get_active_note_for_track(static_cast<uint8_t>(track_idx));
      trigger_note_on(static_cast<uint8_t>(track_idx), note_to_play,
                      drum::config::drumpad::RETRIGGER_VELOCITY);
      record_pad_hit_trace(static_cast<uint8_t>(track_idx));
    }
  }

  // Advance random offset indices when REPEAT + RANDOM are both active
  // Only cycle through offsets for light press (mode 1), freeze on hard press
  // (mode 2)
  if (repeat_effect_.is_active()) {
    random_effect_.advance_offset_indices(num_tracks,
                                          repeat_effect_.get_length());
  }

  // Increment scheduled_step_counter_ AFTER processing the step
  scheduled_step_counter_++;
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::initialize_active_notes() {
  for (size_t track_idx = 0; track_idx < NumTracks; ++track_idx) {
    if (track_idx < config::track_ranges.size()) {
      track_states_[track_idx].active_note =
          config::track_ranges[track_idx].low_note;
    }
  }
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::initialize_all_sequencers() {
  for (size_t track_idx = 0; track_idx < NumTracks; ++track_idx) {
    uint8_t initial_note = track_states_[track_idx].active_note;
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
  for (auto &track_state : track_states_) {
    track_state.just_played_step = std::nullopt;
    track_state.has_velocity_hit = false;
  }
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
    state.active_notes[track_idx] = track_states_[track_idx].active_note;
  }
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::apply_persistent_state(
    const SequencerPersistentState &state) {
  // Apply active notes first
  for (size_t track_idx = 0;
       track_idx < NumTracks && track_idx < config::NUM_TRACKS; ++track_idx) {
    track_states_[track_idx].active_note = state.active_notes[track_idx];
  }

  // Apply per-step velocities to main sequencer; note is derived from active
  // note
  for (size_t track_idx = 0;
       track_idx < NumTracks && track_idx < config::NUM_TRACKS; ++track_idx) {
    auto &track = main_sequencer_.get_track(track_idx);
    uint8_t track_note = track_states_[track_idx].active_note;
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
  SequencerPersistentState state;
  create_persistent_state(state);
  bool success = persistence_.save(state);
  if (success) {
    logger_.info("Manual save to flash completed successfully");
  } else {
    logger_.error("Manual save to flash failed");
  }
  return success;
}

template <size_t NumTracks, size_t NumSteps>
bool SequencerController<NumTracks, NumSteps>::load_state_from_flash() {
  SequencerPersistentState state;
  if (persistence_.load(state)) {
    apply_persistent_state(state);
    logger_.info("Manual load from flash completed successfully");
    return true;
  }
  logger_.warn("Manual load from flash failed - no valid state found");
  return false;
}

template <size_t NumTracks, size_t NumSteps>
bool SequencerController<NumTracks, NumSteps>::init_persistence() {
  if (!persistence_.init()) {
    return true; // Already initialized
  }

  // Attempt to load existing state
  SequencerPersistentState loaded_state;
  if (persistence_.load(loaded_state)) {
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
  return persistence_.is_initialized();
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::mark_state_dirty_public() {
  persistence_.mark_dirty();
}

template <size_t NumTracks, size_t NumSteps>
SequencerPersistentState
SequencerController<NumTracks, NumSteps>::get_current_state() const {
  SequencerPersistentState state;
  create_persistent_state(state);
  return state;
}

template <size_t NumTracks, size_t NumSteps>
bool SequencerController<NumTracks, NumSteps>::apply_state(
    const SequencerPersistentState &state) {
  if (!state.is_valid()) {
    logger_.error("SequencerController: Cannot apply invalid state");
    return false;
  }

  apply_persistent_state(state);
  mark_state_dirty_public();

  logger_.info("SequencerController: State applied successfully");
  return true;
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::enable_random_offset_mode() {
  random_effect_.request_state(RandomEffectState::OffsetActive,
                               is_repeat_active());
  random_effect_.regenerate_offsets(main_sequencer_.get_num_steps(), NumTracks);
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::disable_random_offset_mode() {
  random_effect_.request_state(RandomEffectState::Inactive, is_repeat_active());
  random_intends_flip_ = false;
}

template <size_t NumTracks, size_t NumSteps>
[[nodiscard]] bool
SequencerController<NumTracks, NumSteps>::is_random_offset_mode_active() const {
  return random_effect_.is_offset_mode_enabled();
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::regenerate_random_offsets() {
  random_effect_.regenerate_offsets(main_sequencer_.get_num_steps(), NumTracks);
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks,
                         NumSteps>::trigger_random_hard_press_behavior() {
  // Do everything light press does: enable random offset mode
  if (!random_effect_.is_offset_mode_enabled()) {
    enable_random_offset_mode();
  } else {
    regenerate_random_offsets();
  }

  // Additionally enable probability flipping for hard press
  random_effect_.request_state(RandomEffectState::OffsetWithFlip,
                               is_repeat_active());
  random_intends_flip_ = true;
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks,
                         NumSteps>::disable_random_probability_mode() {
  // Simplify: return to inactive when probability is being disabled by UI
  random_effect_.request_state(RandomEffectState::Inactive, is_repeat_active());
  random_intends_flip_ = false;
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks,
                         NumSteps>::trigger_random_steps_when_stopped() {
  if (is_running()) {
    return; // Only trigger when stopped
  }

  // Generate random steps and store them for highlighting
  const size_t num_steps = main_sequencer_.get_num_steps();
  random_effect_.trigger_step_highlighting(num_steps, NumTracks);
  random_effect_.start_step_highlighting();

  // Play the generated random steps
  for (size_t track_idx = 0; track_idx < NumTracks; ++track_idx) {
    auto random_step_opt =
        random_effect_.get_highlighted_step_for_track(track_idx);
    if (!random_step_opt.has_value()) {
      continue;
    }

    size_t random_step_index = random_step_opt.value();
    auto &track = main_sequencer_.get_track(track_idx);
    auto &step = track.get_step(random_step_index);

    if (step.enabled && step.note.has_value() && step.velocity.has_value()) {
      uint8_t track_index_u8 = static_cast<uint8_t>(track_idx);
      drum::Events::NoteEvent note_on_event{.track_index = track_index_u8,
                                            .note = step.note.value(),
                                            .velocity = step.velocity.value()};
      this->notify_observers(note_on_event);
    }
  }
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks,
                         NumSteps>::start_random_step_highlighting() {
  if (is_running()) {
    return; // Only highlight when stopped
  }

  random_effect_.start_step_highlighting();
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::stop_random_step_highlighting() {
  random_effect_.stop_step_highlighting();
}

template <size_t NumTracks, size_t NumSteps>
[[nodiscard]] bool
SequencerController<NumTracks, NumSteps>::are_random_steps_highlighted() const {
  return random_effect_.are_steps_highlighted();
}

template <size_t NumTracks, size_t NumSteps>
[[nodiscard]] size_t
SequencerController<NumTracks, NumSteps>::get_highlighted_random_step_for_track(
    size_t track_idx) const {
  auto step_opt = random_effect_.get_highlighted_step_for_track(track_idx);
  return step_opt.value_or(get_current_step());
}

template class SequencerController<config::NUM_TRACKS,
                                   config::NUM_STEPS_PER_TRACK>;

} // namespace drum

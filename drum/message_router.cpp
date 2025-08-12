#include "message_router.h"
#include "config.h" // For drum::config::drumpad::track_note_ranges and NUM_TRACKS
#include "musin/midi/midi_wrapper.h" // For MIDI:: calls
#include "musin/ports/pico/libraries/arduino_midi_library/src/midi_Defs.h"
#include "sequencer_controller.h" // For SequencerController
#include <algorithm>              // For std::clamp, std::find
#include <cmath>                  // For std::round
#include <cstdint>                // For uint8_t

namespace drum {

namespace { // Anonymous namespace for internal linkage

void send_midi_cc([[maybe_unused]] const uint8_t channel,
                  [[maybe_unused]] const uint8_t cc_number,
                  [[maybe_unused]] const uint8_t value) {
  // This function is no longer used directly for sending MIDI CCs from
  // MessageRouter as MidiSender will handle it.
  // MIDI::sendControlChange(cc_number, value, channel);
}

void send_midi_note([[maybe_unused]] const uint8_t channel,
                    [[maybe_unused]] const uint8_t note_number,
                    [[maybe_unused]] const uint8_t velocity) {
  // This function is no longer used directly for sending MIDI notes from
  // MessageRouter as MidiSender will handle it. MIDI::sendNoteOn(note_number,
  // velocity, channel);
}

struct ParameterMapping {
  Parameter param_id;
  std::optional<uint8_t> track_index;
};

std::optional<ParameterMapping> map_midi_cc_to_parameter(uint8_t cc_number) {
  // Global Parameters
  switch (cc_number) {
  case 7:
    return {{Parameter::VOLUME, std::nullopt}};
  case 9:
    return {{Parameter::SWING, std::nullopt}};
  case 12:
    return {{Parameter::CRUSH_EFFECT, std::nullopt}};
  case 15:
    return {{Parameter::TEMPO, std::nullopt}};
  case 16:
    return {{Parameter::RANDOM_EFFECT, std::nullopt}};
  case 17:
    return {{Parameter::REPEAT_EFFECT, std::nullopt}};
  case 74:
    return {{Parameter::FILTER_FREQUENCY, std::nullopt}};
  case 75:
    return {{Parameter::FILTER_RESONANCE, std::nullopt}};
  }

  // Per-track Parameters
  if (cc_number >= 21 && cc_number <= 24) {
    return {{Parameter::PITCH, static_cast<uint8_t>(cc_number - 21)}};
  }

  return std::nullopt;
}

} // namespace

// --- MIDI CC Mapping ---
constexpr uint8_t map_parameter_to_midi_cc(Parameter param_id,
                                           std::optional<uint8_t> track_index) {
  switch (param_id) {
  case Parameter::PITCH:
    if (track_index.has_value()) {
      switch (track_index.value()) {
      case 0:
        return 21; // Track 1 Pitch CC (DATO Chart)
      case 1:
        return 22; // Track 2 Pitch CC (DATO Chart)
      case 2:
        return 23; // Track 3 Pitch CC (DATO Chart)
      case 3:
        return 24; // Track 4 Pitch CC (DATO Chart)
      default:
        break;
      }
    }
    return 0; // Invalid or no track index for pitch

  // Global Parameters from MIDI Chart
  case Parameter::VOLUME:
    return 7;
  case Parameter::SWING:
    return 9;
  case Parameter::CRUSH_EFFECT:
    return 12;
  case Parameter::TEMPO:
    return 15;
  case Parameter::RANDOM_EFFECT:
    return 16;
  case Parameter::REPEAT_EFFECT:
    return 17;
  case Parameter::FILTER_FREQUENCY:
    return 74;
  case Parameter::FILTER_RESONANCE:
    return 75;
  }
  return 0;
}

// --- MessageRouter Implementation ---

MessageRouter::MessageRouter(
    AudioEngine &audio_engine,
    SequencerController<drum::config::NUM_TRACKS,
                        drum::config::NUM_STEPS_PER_TRACK>
        &sequencer_controller,
    musin::midi::MidiSender &midi_sender, musin::Logger &logger)
    : _audio_engine(audio_engine), _sequencer_controller(sequencer_controller),
      _midi_sender(midi_sender), logger_(logger),
      _output_mode(OutputMode::BOTH), _local_control_mode(LocalControlMode::ON),
      _previous_local_control_mode(
          std::nullopt) { // Default local control to ON
  note_event_queue_.clear();
  // TODO: Initialize _track_sample_map if added
}

void MessageRouter::set_output_mode(OutputMode mode) {
  _output_mode = mode;
}

OutputMode MessageRouter::get_output_mode() const {
  return _output_mode;
}

void MessageRouter::set_local_control_mode(LocalControlMode mode) {
  _local_control_mode = mode;
}

LocalControlMode MessageRouter::get_local_control_mode() const {
  return _local_control_mode;
}

void MessageRouter::trigger_sound(uint8_t track_index, uint8_t midi_note,
                                  uint8_t velocity) {
  if (track_index >= 4)
    return;

  drum::Events::NoteEvent event{
      .track_index = track_index, .note = midi_note, .velocity = velocity};
  notification(event);
}

void MessageRouter::set_parameter(Parameter param_id, float value,
                                  std::optional<uint8_t> track_index) {

  if (param_id == Parameter::PITCH &&
      (!track_index.has_value() || track_index.value() >= config::NUM_TRACKS)) {
    return;
  }

  value = std::clamp(value, 0.0f, 1.0f);

  // Notify observers about the parameter change.
  drum::Events::ParameterChangeEvent event{param_id, value, track_index};
  etl::observable<etl::observer<drum::Events::ParameterChangeEvent>,
                  2>::notify_observers(event);

  if (_output_mode == OutputMode::MIDI || _output_mode == OutputMode::BOTH) {
    uint8_t cc_number = map_parameter_to_midi_cc(param_id, track_index);
    if (cc_number > 0) {
      uint8_t midi_channel = drum::config::FALLBACK_MIDI_CHANNEL;
      uint8_t midi_value = static_cast<uint8_t>(std::round(value * 127.0f));
      midi_value = std::min(midi_value, static_cast<uint8_t>(127));
      _midi_sender.sendControlChange(midi_channel, cc_number, midi_value);
      // TODO: Future enhancement - Add logic here to send 14-bit CC if desired
    }
  }

  if ((_output_mode == OutputMode::AUDIO || _output_mode == OutputMode::BOTH) &&
      _local_control_mode == LocalControlMode::ON) {
    switch (param_id) {
    case Parameter::PITCH:
      _audio_engine.set_pitch(track_index.value(), value);
      break;
    case Parameter::FILTER_FREQUENCY:
      _audio_engine.set_filter_frequency(value);
      break;
    case Parameter::FILTER_RESONANCE:
      _audio_engine.set_filter_resonance(value);
      break;
    case Parameter::VOLUME:
      _audio_engine.set_volume(value);
      break;
    case Parameter::CRUSH_EFFECT: {
      // AudioEngine::set_crush_depth expects a normalized value (0.0 to 1.0)
      // It internally maps this to bit depth (5 to 16).
      // Higher normalized value should mean more crush (lower bit depth).
      // The map_value_linear in AudioEngine for crush_depth is
      // (normalized_value, 5.0f, 16.0f) So a higher normalized_value gives a
      // higher bit depth (less crush). This is inverted from typical "amount".
      // To make higher CC value = more crush, we pass (1.0f - value) to
      // set_crush_depth.
      _audio_engine.set_crush_depth(1.0f - value);
      _audio_engine.set_crush_rate(value);
      break;
    }
    case Parameter::SWING:
      // TODO: Implement swing effect in sequencer/audio path
      break;
    case Parameter::TEMPO:
      // TODO: Implement tempo change in sequencer/audio path
      break;
    case Parameter::RANDOM_EFFECT:
      // TODO: Implement random effect in sequencer/audio path
      break;
    case Parameter::REPEAT_EFFECT:
      // TODO: Implement repeat effect in sequencer/audio path
      break;
    }
  }
}

void MessageRouter::update() {
  while (!note_event_queue_.empty()) {
    drum::Events::NoteEvent event = note_event_queue_.front();
    note_event_queue_.pop();
    logger_.debug("MessageRouter processing NoteEvent",
                  static_cast<uint32_t>(event.track_index));

    if (_output_mode == OutputMode::MIDI || _output_mode == OutputMode::BOTH) {
      _midi_sender.sendNoteOn(drum::config::FALLBACK_MIDI_CHANNEL, event.note,
                              event.velocity);
    }

    if ((_output_mode == OutputMode::AUDIO ||
         _output_mode == OutputMode::BOTH) &&
        _local_control_mode == LocalControlMode::ON) {
      // Notify observers like AudioEngine and PizzaDisplay to handle the event
      // locally
      etl::observable<
          etl::observer<drum::Events::NoteEvent>,
          drum::config::MAX_NOTE_EVENT_OBSERVERS>::notify_observers(event);
    }
  }
}

// --- MessageRouter Notification Implementation ---

void MessageRouter::notification(drum::Events::NoteEvent event) {
  if (note_event_queue_.full()) {
    logger_.warn("Note event queue full, dropping event.");
    return;
  }
  note_event_queue_.push(event);
}

void MessageRouter::notification(
    drum::Events::SysExTransferStateChangeEvent event) {
  if (event.is_active) {
    _audio_engine.mute();
    _previous_local_control_mode = get_local_control_mode();
    set_local_control_mode(LocalControlMode::OFF);
  } else {
    _audio_engine.unmute();
    if (_previous_local_control_mode.has_value()) {
      set_local_control_mode(_previous_local_control_mode.value());
      _previous_local_control_mode.reset();
    }
  }
}

void MessageRouter::handle_incoming_note_on(uint8_t note, uint8_t velocity) {
  for (size_t track_idx = 0; track_idx < drum::config::track_note_ranges.size();
       ++track_idx) {
    if (track_idx >= drum::config::NUM_TRACKS)
      break; // Ensure we don't go out of bounds if config sizes differ

    const auto &notes_for_track = drum::config::track_note_ranges[track_idx];
    auto it = std::find(notes_for_track.begin(), notes_for_track.end(), note);

    if (it != notes_for_track.end()) {
      // Note found for this track.
      // Queue the event to be processed in the main loop.
      drum::Events::NoteEvent event{.track_index =
                                        static_cast<uint8_t>(track_idx),
                                    .note = note,
                                    .velocity = velocity};
      notification(event);

      // Set the active note for that track in the sequencer controller.
      _sequencer_controller.set_active_note_for_track(
          static_cast<uint8_t>(track_idx), note);
      // Assuming a note belongs to only one track's list for this purpose, so
      // we can stop.
      return;
    }
  }
}

void MessageRouter::handle_incoming_note_off(uint8_t note, uint8_t velocity) {
  // Currently, we do nothing for note-off messages.
  // This is where logic to mute a voice would go if desired.
  (void)note;
  (void)velocity;
}

void MessageRouter::handle_incoming_midi_cc(uint8_t controller, uint8_t value) {
  auto mapping = map_midi_cc_to_parameter(controller);
  if (mapping.has_value()) {
    float normalized_value = static_cast<float>(value) / 127.0f;
    set_parameter(mapping->param_id, normalized_value, mapping->track_index);
  }
}

} // namespace drum

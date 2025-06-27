#include "sound_router.h"
#include "config.h"                  // For drum::config::drumpad::track_note_ranges and NUM_TRACKS
#include "musin/midi/midi_wrapper.h" // For MIDI:: calls
#include "musin/ports/pico/libraries/arduino_midi_library/src/midi_Defs.h"
#include "sequencer_controller.h" // For SequencerController
#include <algorithm>              // For std::clamp, std::find
#include <cmath>                  // For std::round
#include <cstdint>                // For uint8_t

namespace { // Anonymous namespace for internal linkage

void send_midi_cc(const uint8_t channel, const uint8_t cc_number, const uint8_t value) {
  MIDI::sendControlChange(cc_number, value, channel);
}

void send_midi_note(const uint8_t channel, const uint8_t note_number, const uint8_t velocity) {
  // The underlying library handles Note On/Off based on velocity
  // Use sendNoteOn for both Note On (velocity > 0) and Note Off (velocity == 0)
  MIDI::sendNoteOn(note_number, velocity, channel);
}

} // namespace

namespace drum {

// --- MIDI CC Mapping ---
constexpr uint8_t map_parameter_to_midi_cc(Parameter param_id, std::optional<uint8_t> track_index) {
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

// --- SoundRouter Implementation ---

SoundRouter::SoundRouter(
    AudioEngine &audio_engine,
    SequencerController<drum::config::NUM_TRACKS, drum::config::NUM_STEPS_PER_TRACK>
        &sequencer_controller)
    : _audio_engine(audio_engine), _sequencer_controller(sequencer_controller),
      _output_mode(OutputMode::BOTH),
      _local_control_mode(LocalControlMode::ON) { // Default local control to ON
  // TODO: Initialize _track_sample_map if added
}

void SoundRouter::set_output_mode(OutputMode mode) {
  _output_mode = mode;
}

OutputMode SoundRouter::get_output_mode() const {
  return _output_mode;
}

void SoundRouter::set_local_control_mode(LocalControlMode mode) {
  _local_control_mode = mode;
}

LocalControlMode SoundRouter::get_local_control_mode() const {
  return _local_control_mode;
}

void SoundRouter::trigger_sound(uint8_t track_index, uint8_t midi_note, uint8_t velocity) {
  if (track_index >= 4)
    return;

  if (_output_mode == OutputMode::MIDI || _output_mode == OutputMode::BOTH) {
    // Send MIDI notes on the configured default MIDI channel
    send_midi_note(drum::config::FALLBACK_MIDI_CHANNEL, midi_note, velocity);
  }

  if (_output_mode == OutputMode::AUDIO || _output_mode == OutputMode::BOTH) {
    const auto &defs = drum::config::global_note_definitions;
    auto it = std::find_if(defs.begin(), defs.end(), [midi_note](const auto &def) {
      return def.midi_note_number == midi_note;
    });

    if (it != defs.end()) {
      uint32_t sample_id = std::distance(defs.begin(), it);

      if (velocity > 0) {
        _audio_engine.play_on_voice(track_index, sample_id, velocity);
      } // else {
      //   _audio_engine.stop_voice(track_index);
      // }
    }
  }
}

void SoundRouter::set_parameter(Parameter param_id, float value,
                                std::optional<uint8_t> track_index) {

  if (param_id == Parameter::PITCH &&
      (!track_index.has_value() || track_index.value() >= config::NUM_TRACKS)) {
    return;
  }

  value = std::clamp(value, 0.0f, 1.0f);

  if (_output_mode == OutputMode::MIDI || _output_mode == OutputMode::BOTH) {
    uint8_t cc_number = map_parameter_to_midi_cc(param_id, track_index);
    if (cc_number > 0) {
      uint8_t midi_channel = drum::config::FALLBACK_MIDI_CHANNEL;
      uint8_t midi_value = static_cast<uint8_t>(std::round(value * 127.0f));
      midi_value = std::min(midi_value, static_cast<uint8_t>(127));
      send_midi_cc(midi_channel, cc_number, midi_value);
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
      // The map_value_linear in AudioEngine for crush_depth is (normalized_value, 5.0f, 16.0f)
      // So a higher normalized_value gives a higher bit depth (less crush). This is inverted from
      // typical "amount". To make higher CC value = more crush, we pass (1.0f - value) to
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

// --- SoundRouter Notification Implementation ---

void SoundRouter::notification(drum::Events::NoteEvent event) {
  trigger_sound(event.track_index, event.note, event.velocity);
}

void SoundRouter::handle_incoming_midi_note(uint8_t note, uint8_t velocity) {
  for (size_t track_idx = 0; track_idx < drum::config::track_note_ranges.size(); ++track_idx) {
    if (track_idx >= drum::config::NUM_TRACKS)
      break; // Ensure we don't go out of bounds if config sizes differ

    const auto &notes_for_track = drum::config::track_note_ranges[track_idx];
    auto it = std::find(notes_for_track.begin(), notes_for_track.end(), note);

    if (it != notes_for_track.end()) {
      // Note found for this track.
      // Play the sound on the audio engine for this track.
      // The AudioEngine::play_on_voice should handle velocity 0 as note off.

      // Notify observers (like PizzaDisplay) about this note event
      drum::Events::NoteEvent event{
          .track_index = static_cast<uint8_t>(track_idx), .note = note, .velocity = velocity};
      this->notify_observers(event);

      // Set the active note for that track in the sequencer controller,
      // only if it's a Note On (velocity > 0).
      if (velocity > 0) {
        _sequencer_controller.set_active_note_for_track(static_cast<uint8_t>(track_idx), note);
      }
      // Assuming a note belongs to only one track's list for this purpose, so we can stop.
      return;
    }
  }
}

} // namespace drum

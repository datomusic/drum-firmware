#include "sound_router.h"
#include "musin/midi/midi_wrapper.h" // For MIDI:: calls
#include "musin/ports/pico/libraries/arduino_midi_library/src/midi_Defs.h"
#include <algorithm> // For std::clamp
#include <cstdint>   // For uint8_t

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

namespace SB25 {

// --- MIDI CC Mapping ---
constexpr uint8_t map_parameter_to_midi_cc(Parameter param_id, std::optional<uint8_t> track_index) {
  switch (param_id) {
  case Parameter::DRUM_PRESSURE_1:
    return 20;
  case Parameter::DRUM_PRESSURE_2:
    return 21;
  case Parameter::DRUM_PRESSURE_3:
    return 22;
  case Parameter::DRUM_PRESSURE_4:
    return 23;
  case Parameter::PITCH:
    if (track_index.has_value()) {
      switch (track_index.value()) {
      case 0:
        return 16;
      case 1:
        return 17;
      case 2:
        return 18;
      case 3:
        return 19;
      default:
        break;
      }
    }
    return 0;

  case Parameter::FILTER_FREQUENCY:
    return 75;
  case Parameter::FILTER_RESONANCE:
    return 76;
  case Parameter::VOLUME:
    return 7;
  case Parameter::CRUSH_RATE:
    return 77;
  case Parameter::CRUSH_DEPTH:
    return 78;
  }
  return 0;
}

// --- SoundRouter Implementation ---

SoundRouter::SoundRouter(AudioEngine &audio_engine)
    : _audio_engine(audio_engine), _output_mode(OutputMode::BOTH) {
  // TODO: Initialize _track_sample_map if added
}

void SoundRouter::set_output_mode(OutputMode mode) {
  _output_mode = mode;
}

OutputMode SoundRouter::get_output_mode() const {
  return _output_mode;
}

void SoundRouter::trigger_sound(uint8_t track_index, uint8_t midi_note, uint8_t velocity) {
  if (track_index >= 4)
    return;

  if (_output_mode == OutputMode::MIDI || _output_mode == OutputMode::BOTH) {
    send_midi_note(static_cast<uint8_t>(track_index + 1), midi_note, velocity);
  }

  if (_output_mode == OutputMode::AUDIO || _output_mode == OutputMode::BOTH) {
    // TODO: Get the correct sample_id based on track_index (using _track_sample_map)
    uint32_t sample_id = midi_note % 32; // Placeholder

    if (velocity > 0) {
      _audio_engine.play_on_voice(track_index, sample_id, velocity);
    } else {
      _audio_engine.stop_voice(track_index);
    }
  }
}

void SoundRouter::set_parameter(Parameter param_id, float value,
                                std::optional<uint8_t> track_index) {

  if ((param_id == Parameter::PITCH || param_id == Parameter::DRUM_PRESSURE_1 ||
       param_id == Parameter::DRUM_PRESSURE_2 || param_id == Parameter::DRUM_PRESSURE_3 ||
       param_id == Parameter::DRUM_PRESSURE_4) &&
      (!track_index.has_value() || track_index.value() >= 4)) {
    return;
  }

  value = std::clamp(value, 0.0f, 1.0f);

  if (_output_mode == OutputMode::MIDI || _output_mode == OutputMode::BOTH) {
    uint8_t cc_number = map_parameter_to_midi_cc(param_id, track_index);
    if (cc_number > 0) {
      uint8_t midi_channel = 1;
      if (track_index.has_value()) {
        midi_channel = track_index.value() + 1;
      }
      uint8_t midi_value = static_cast<uint8_t>(std::round(value * 127.0f));
      midi_value = std::min(midi_value, static_cast<uint8_t>(127));
      send_midi_cc(midi_channel, cc_number, midi_value);
      // TODO: Future enhancement - Add logic here to send 14-bit CC if desired
    }
  }

  if (_output_mode == OutputMode::AUDIO || _output_mode == OutputMode::BOTH) {
    switch (param_id) {
    case Parameter::DRUM_PRESSURE_1:
      // TODO: Map DRUM_PRESSURE_1 to a specific voice effect ID and call audio engine
      // Example: _audio_engine.set_voice_effect_parameter(track_index.value(),
      // EFFECT_ID_VOICE_DRUM1, value);
      break;
    case Parameter::DRUM_PRESSURE_2:
      // TODO: Map DRUM_PRESSURE_2
      break;
    case Parameter::DRUM_PRESSURE_3:
      // TODO: Map DRUM_PRESSURE_3
      break;
    case Parameter::DRUM_PRESSURE_4:
      // TODO: Map DRUM_PRESSURE_4
      break;
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
    case Parameter::CRUSH_RATE:
      _audio_engine.set_crush_rate(value);
      break;
    case Parameter::CRUSH_DEPTH: {
      uint8_t depth = 1 + static_cast<uint8_t>(std::round((1.0f - value) * 15.0f));
      depth = std::clamp(depth, static_cast<uint8_t>(1), static_cast<uint8_t>(16));
      _audio_engine.set_crush_depth(depth);
      break;
    }
    }
  }
}

// --- SoundRouter Notification Implementation ---

void SoundRouter::notification(SB25::Events::NoteEvent event) {
  trigger_sound(event.track_index, event.note, event.velocity);
}

} // namespace SB25

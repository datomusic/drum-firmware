#include "audio_engine.h"

#include "config.h"
#include "musin/audio/audio_output.h"
#include "musin/hal/debug_utils.h"
#include "sample_repository.h"
#include "sample_slot_manager.h"

#include <algorithm>
#include <cmath>

namespace drum {

namespace {

float map_value_linear(float normalized_value, float min_val, float max_val) {
  normalized_value = std::clamp(normalized_value, 0.0f, 1.0f);
  return std::lerp(min_val, max_val, normalized_value);
}

float map_value_breakpoint(float normalized_value, float min_val,
                           float breakpoint_val, float max_val) {
  normalized_value = std::clamp(normalized_value, 0.0f, 1.0f);

  constexpr float breakpoint_input = 0.5f;
  if (normalized_value <= breakpoint_input) {
    const float t = normalized_value / breakpoint_input;
    return std::lerp(min_val, breakpoint_val, t);
  } else {
    const float t =
        (normalized_value - breakpoint_input) / (1.0f - breakpoint_input);
    return std::lerp(breakpoint_val, max_val, t);
  }
}

float map_value_pitch_fast(float normalized_value) {
  normalized_value = std::clamp(normalized_value, 0.0f, 1.0f);
  return 0.5f + normalized_value * (0.5f + normalized_value);
}

float map_value_filter_fast(float normalized_value) {
  const float inverted_value = 1.0f - std::clamp(normalized_value, 0.0f, 1.0f);
  return map_value_breakpoint(inverted_value, 400.0f, 800.0f, 20000.0f);
}

} // namespace

AudioEngine::Voice::Voice() : sound(reader) {
}

AudioEngine::AudioEngine(const SampleRepository &repository,
                         SampleSlotManager &slot_manager, musin::Logger &logger)
    : sample_repository_(repository), slot_manager_(slot_manager),
      logger_(logger), voice_sources_{&voices_[0].sound, &voices_[1].sound,
                                      &voices_[2].sound, &voices_[3].sound},
      mixer_(voice_sources_), crusher_(mixer_), lowpass_(crusher_),
      highpass_(lowpass_) {
  // Initialize to a known, silent state.
  set_volume(1.0f); // Set master volume to full.

  // Set filters to neutral positions.
  set_filter_frequency(20000.0f);   // Fully open.
  set_filter_resonance(0.0f);       // No resonance.
  highpass_.filter.frequency(0.0f); // Fully open.
  highpass_.filter.resonance(0.7f); // Default resonance.

  // Set crusher to be transparent.
  set_crush_depth(1.0f); // Maximum bit depth (i.e., no crush).
  set_crush_rate(1.0f);  // Maximum sample rate (i.e., no crush).

  // Initialize all voice gains to zero to ensure silence.
  for (size_t i = 0; i < NUM_VOICES; ++i) {
    mixer_.gain(i, 0.25f);
  }
}

bool AudioEngine::init() {
  if (is_initialized_) {
    return true;
  }

  if (!AudioOutput::init()) {
    return false;
  }
  is_initialized_ = true;
  return true;
}

void AudioEngine::deinit() {
  if (is_initialized_) {
    AudioOutput::deinit();
    is_initialized_ = false;
  }
}

void AudioEngine::process() {
  AudioOutput::update(highpass_);
}

void AudioEngine::pump_sample_loads() {
  slot_manager_.update();

  // Commit a finished load as soon as the target voice is no longer
  // sounding, so the next trigger plays the new sample immediately.
  for (uint8_t voice_index = 0; voice_index < NUM_VOICES; ++voice_index) {
    if (slot_manager_.staging_ready_for_voice(voice_index) &&
        !voices_[voice_index].reader.has_data()) {
      slot_manager_.commit_staging();
      break;
    }
  }
}

void AudioEngine::play_on_voice(uint8_t voice_index, size_t sample_index,
                                uint8_t velocity) {
  musin::hal::DebugUtils::ScopedProfile p(
      musin::hal::DebugUtils::g_section_profiler,
      static_cast<size_t>(ProfileSection::PLAY_ON_VOICE_UPDATE));
  if (!is_initialized_ || voice_index >= NUM_VOICES) {
    return;
  }

  if (velocity == 0) {
    if constexpr (!config::IGNORE_MIDI_NOTE_OFF) {
      stop_voice(voice_index);
    }
    return;
  }

  Voice &voice = voices_[voice_index];

  if (!slot_manager_.voice_has_sample(voice_index, sample_index)) {
    if (slot_manager_.staging_ready_for(voice_index, sample_index)) {
      slot_manager_.commit_staging();
    } else {
      // Start loading; this trigger replays the voice's current sample.
      auto path_opt = sample_repository_.get_path(sample_index);
      if (path_opt.has_value()) {
        slot_manager_.request_load(voice_index, sample_index, *path_opt);
      }
      musin::hal::DebugUtils::g_sample_load_misses++;
    }
  }

  if (slot_manager_.voice_length(voice_index) == 0) {
    return;
  }
  voice.reader.set_source(slot_manager_.voice_data(voice_index),
                          slot_manager_.voice_length(voice_index));

  const float normalized_velocity = static_cast<float>(velocity) / 127.0f;
  const float gain = map_value_linear(normalized_velocity, 0.0f, 1.0f);
  mixer_.gain(voice_index, gain);

  voice.sound.play(voice.current_pitch);
}

void AudioEngine::stop_voice(uint8_t voice_index) {
  if (!is_initialized_ || voice_index >= NUM_VOICES) {
    return;
  }
  mixer_.gain(voice_index, 0.0f);
  // TODO: Consider if voice.sound needs a reset/stop method for efficiency
}

void AudioEngine::set_pitch(uint8_t voice_index, float value) {
  if (!is_initialized_ || voice_index >= NUM_VOICES) {
    return;
  }

  const float pitch_multiplier = map_value_pitch_fast(value);
  voices_[voice_index].current_pitch = pitch_multiplier;
  // TODO: Consider if pitch should affect currently playing sound (requires
  // Sound modification) voices_[voice_index].sound.set_speed(pitch_multiplier);
}

void AudioEngine::set_volume(float volume) {
  // Clamp volume to [0.0, 1.0]
  current_volume_ = std::clamp(volume, 0.0f, 1.0f);
  if (!muted_) {
    AudioOutput::volume(current_volume_);
  }
}

void AudioEngine::mute() {
  if (!muted_) {
    muted_ = true;
    AudioOutput::mute();
  }
}

void AudioEngine::unmute() {
  if (muted_) {
    muted_ = false;
    AudioOutput::unmute();
  }
}

void AudioEngine::set_filter_frequency(float normalized_value) {

  const float freq = map_value_filter_fast(normalized_value);
  lowpass_.filter.frequency(freq);
}
void AudioEngine::set_filter_resonance(float normalized_value) {
  normalized_value = std::clamp(normalized_value, 0.0f, 1.0f);
  const float resonance = map_value_linear(normalized_value, 0.7f, 3.0f);
  lowpass_.filter.resonance(resonance);
}
void AudioEngine::set_crush_rate(float normalized_value) {
  const float inverted_value = 1.0f - std::clamp(normalized_value, 0.0f, 1.0f);
  const float rate =
      map_value_breakpoint(inverted_value, 882.0f, 2205.0f, 22050.0f);
  crusher_.sampleRate(rate);
}

void AudioEngine::set_crush_depth(float normalized_value) {
  normalized_value = std::clamp(normalized_value, 0.0f, 1.0f);
  const uint8_t depth = map_value_linear(normalized_value, 5.0f, 16.0f);
  crusher_.bits(depth);
}

void AudioEngine::notification(drum::Events::NoteEvent event) {
  // Direct mapping: MIDI note = sample slot
  size_t sample_id = event.note;
  play_on_voice(event.track_index, sample_id, event.velocity);
}

} // namespace drum

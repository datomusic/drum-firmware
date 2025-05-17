#include "audio_engine.h"

#include "etl/array.h"

#include "musin/audio/audio_output.h"
#include "musin/audio/crusher.h"
#include "musin/audio/filter.h"
#include "musin/audio/mixer.h"
#include "musin/audio/sound.h"

#include "sb25_samples.h"

#include <algorithm>
#include <cmath>

namespace drum {

namespace {

float map_value_linear(float normalized_value, float min_val, float max_val) {
  normalized_value = std::clamp(normalized_value, 0.0f, 1.0f);
  return std::lerp(min_val, max_val, normalized_value);
}

float map_value_pitch_fast(float normalized_value) {
  normalized_value = std::clamp(normalized_value, 0.0f, 1.0f);
  return 0.5f + normalized_value * (0.5f + normalized_value);
}

float map_value_filter_fast(float normalized_value) {
  normalized_value = std::clamp(normalized_value, 0.0f, 1.0f);
  const float min_freq = 400.0f;
  const float max_freq = 20000.0f;
  const float range = max_freq - min_freq;
  const float inverted_normalized_value = 1.0f - normalized_value;
  return min_freq +
         range * inverted_normalized_value * inverted_normalized_value * inverted_normalized_value;
}

} // namespace

AudioEngine::Voice::Voice() : sound(reader.emplace()) { // reader is default constructed here
}

AudioEngine::AudioEngine()
    : voice_sources_{&voices_[0].sound, &voices_[1].sound, &voices_[2].sound, &voices_[3].sound},
      mixer_(voice_sources_), crusher_(mixer_), lowpass_(crusher_), highpass_(lowpass_) {
  lowpass_.filter.frequency(20000.0f);
  lowpass_.filter.resonance(1.0f);
  highpass_.filter.frequency(100.0f);
  lowpass_.filter.resonance(0.7f);
  crusher_.sampleRate(static_cast<float>(AudioOutput::SAMPLE_FREQUENCY));
  crusher_.bits(16);

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
  AudioOutput::route_line_in_to_headphone(true);
  is_initialized_ = true;
  return true;
}

void AudioEngine::process() {

  AudioOutput::update(highpass_);
}

void AudioEngine::play_on_voice(uint8_t voice_index, size_t sample_index, uint8_t velocity) {
  musin::hal::DebugUtils::ScopedProfile p(
      musin::hal::DebugUtils::g_section_profiler,
      static_cast<size_t>(ProfileSection::PLAY_ON_VOICE_UPDATE));
  if (!is_initialized_ || voice_index >= NUM_VOICES) {
    return;
  }

  sample_index = sample_index % 32;

  Voice &voice = voices_[voice_index];

  const musin::SampleData &current_sample_data = all_samples[sample_index];
  voice.reader->set_source(current_sample_data); // Pass the musin::SampleData object directly

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
  // TODO: Consider if pitch should affect currently playing sound (requires Sound modification)
  // voices_[voice_index].sound.set_speed(pitch_multiplier);
}

void AudioEngine::set_volume(float volume) {
  // Clamp volume to [0.0, 1.0] before passing to AudioOutput
  volume = std::clamp(volume, 0.0f, 1.0f);
  AudioOutput::volume(volume);
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
  normalized_value = std::clamp(normalized_value, 0.0f, 1.0f);
  const float rate = map_value_filter_fast(normalized_value);
  crusher_.sampleRate(rate);
}

void AudioEngine::set_crush_depth(float normalized_value) {
  normalized_value = std::clamp(normalized_value, 0.0f, 1.0f);
  const uint8_t depth = map_value_linear(normalized_value, 5.0f, 16.0f);
  crusher_.bits(depth);
}

void AudioEngine::set_decay(uint8_t voice_index, float value) {
  // TODO: Implement decay setting for the specified voice
}
} // namespace drum

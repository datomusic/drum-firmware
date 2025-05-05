#include "sb25_drum/audio_engine.h"

#include "etl/array.h"
#include "pico/time.h" // Include if needed for future timing logic

#include "musin/audio/audio_output.h"
#include "musin/audio/block.h"
#include "musin/audio/crusher.h"
#include "musin/audio/filter.h"
#include "musin/audio/memory_reader.h"
#include "musin/audio/mixer.h"
#include "musin/audio/sound.h"

#include "sb25_samples.h"

#include <algorithm> // Include for std::clamp
#include <cmath>     // Using std::log, std::exp, std::lerp, std::pow

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
  const float min_freq = 100.0f;
  const float max_freq = 20000.0f;
  const float range = max_freq - min_freq;
  const float inverted_normalized_value = 1.0f - normalized_value;
  return min_freq +
         range * inverted_normalized_value * inverted_normalized_value * inverted_normalized_value;
}

} // namespace

AudioEngine::Voice::Voice() : sound(reader.emplace()) {
}

AudioEngine::AudioEngine()
    : voice_sources_{&voices_[0].sound, &voices_[1].sound, &voices_[2].sound, &voices_[3].sound},
      mixer_(voice_sources_), crusher_(mixer_), lowpass_(crusher_) {
  lowpass_.filter.frequency(20000.0f);
  lowpass_.filter.resonance(1.0f);
  crusher_.sampleRate(static_cast<float>(AudioOutput::SAMPLE_FREQUENCY));
  crusher_.bits(16);

  for (size_t i = 0; i < NUM_VOICES; ++i) {
    mixer_.gain(i, 0.7f);
  }
}

bool AudioEngine::init() {
  if (is_initialized_) {
    return true;
  }

  if (!AudioOutput::init()) {
    // TODO: Add proper logging/error indication if available
    return false;
  }
  AudioOutput::route_line_in_to_headphone(true);
  is_initialized_ = true;
  // TODO: Add logging/status indication
  return true;
}

void AudioEngine::process() {
  if (!is_initialized_) {
    return;
  }
  AudioOutput::update(lowpass_);
}

void AudioEngine::play_on_voice(uint8_t voice_index, size_t sample_index, uint8_t velocity) {
  if (!is_initialized_ || voice_index >= NUM_VOICES) {
    return;
  }

  sample_index = sample_index % 32;

  Voice &voice = voices_[voice_index];

  voice.reader->set_source(all_samples[sample_index].data, all_samples[sample_index].length);

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

void AudioEngine::set_voice_effect_parameter(uint8_t voice_index, uint8_t effect_id, float value) {
  if (!is_initialized_ || voice_index >= NUM_VOICES) {
    return;
  }

  switch (effect_id) {
  case EFFECT_ID_VOICE_VOLUME: {
    const float gain = map_value_linear(value, 0.0f, 1.0f);
    mixer_.gain(voice_index, gain);
    break;
  }
  // TODO: Add cases for other per-voice effects (e.g., panning)
  default:
    break;
  }
}

void AudioEngine::set_pitch(uint8_t voice_index, float value) {
  if (!is_initialized_ || voice_index >= NUM_VOICES) {
    return;
  }

  const float pitch_multiplier = map_value_pitch_fast(value);
  printf("set pitch to %f \n", pitch_multiplier);
  voices_[voice_index].current_pitch = pitch_multiplier;
  // TODO: Consider if pitch should affect currently playing sound (requires Sound modification)
  // voices_[voice_index].sound.set_speed(pitch_multiplier);
}

void AudioEngine::set_volume(float volume) {
  if (!is_initialized_) {
    return;
  }
  // Clamp volume to [0.0, 1.0] before passing to AudioOutput
  volume = std::clamp(volume, 0.0f, 1.0f);
  AudioOutput::volume(volume);
}

void AudioEngine::set_filter_frequency(float normalized_value) {
  if (!is_initialized_) {
    return;
  }
  const float freq = map_value_filter_fast(normalized_value);
  lowpass_.filter.frequency(freq);
}
void AudioEngine::set_filter_resonance(float normalized_value) {
  if (!is_initialized_) {
    return;
  }
  normalized_value = std::clamp(normalized_value, 0.0f, 1.0f);
  const float resonance = map_value_linear(normalized_value, 0.7f, 3.0f);
  lowpass_.filter.resonance(resonance);
}
void AudioEngine::set_crush_rate(float normalized_value) {
  if (!is_initialized_) {
    return;
  }
  normalized_value = std::clamp(normalized_value, 0.0f, 1.0f);
  const float rate = map_value_linear(normalized_value,
                                      static_cast<float>(AudioOutput::SAMPLE_FREQUENCY), 2000.0f);
  crusher_.sampleRate(rate);
}

void AudioEngine::set_crush_depth(uint8_t depth) {
  if (!is_initialized_) {
    return;
  }
  crusher_.bits(depth);
}
} // namespace drum

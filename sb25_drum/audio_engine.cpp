#include "sb25_drum/audio_engine.h"

#include "etl/array.h"
#include "etl/math.h"
#include "etl/numerics.h"
#include "pico/time.h" // Include if needed for future timing logic

#include "musin/audio/audio_output.h"
#include "musin/audio/block.h"
#include "musin/audio/crusher.h"
#include "musin/audio/filter.h"
#include "musin/audio/memory_reader.h"
#include "musin/audio/mixer.h"
#include "musin/audio/sound.h"

// This assumes support/all_samples.h defines Musin::all_samples and Musin::num_samples
#include "support/all_samples.h"

namespace SB25 {

namespace {

float map_value_linear(uint8_t value, float min_val, float max_val) {
  const float normalized_value = static_cast<float>(value) / 127.0f;
  return etl::lerp(min_val, max_val, normalized_value);
}

float map_value_to_freq(uint8_t value, float min_freq = 20.0f, float max_freq = 20000.0f) {
  const float normalized_value = static_cast<float>(value) / 127.0f;
  const float log_min = etl::log(min_freq);
  const float log_max = etl::log(max_freq);
  return etl::exp(etl::lerp(log_min, log_max, normalized_value));
}

float map_velocity_to_gain(uint8_t velocity) {
  return map_value_linear(velocity, 0.0f, 1.0f);
}

float map_pitch_value_to_multiplier(uint8_t value) {
    const float semitones = (static_cast<float>(value) - 64.0f) * (12.0f / 64.0f);
    return etl::pow(2.0f, semitones / 12.0f);
}

float map_value_to_crush_rate(uint8_t value) {
    return map_value_linear(value, 2000.0f, static_cast<float>(Musin::AudioOutput::SAMPLE_FREQUENCY));
}

} // namespace

AudioEngine::Voice::Voice() : sound(reader.emplace()) {
}

AudioEngine::AudioEngine()
    : voice_sources_{&voices_[0].sound, &voices_[1].sound, &voices_[2].sound, &voices_[3].sound},
      mixer_(voice_sources_),
      crusher_(mixer_),
      lowpass_(crusher_)
{
  lowpass_.filter.frequency(20000.0f);
  lowpass_.filter.resonance(1.0f);
  crusher_.sampleRate(static_cast<float>(Musin::AudioOutput::SAMPLE_FREQUENCY));
  crusher_.bits(16);

  for (size_t i = 0; i < NUM_VOICES; ++i) {
    mixer_.gain(i, 0.7f);
  }
}

bool AudioEngine::init() {
  if (is_initialized_) {
    return true;
  }

  if (!Musin::AudioOutput::init()) {
    // TODO: Add proper logging/error indication if available
    return false;
  }

  is_initialized_ = true;
  // TODO: Add logging/status indication
  return true;
}

void AudioEngine::process() {
  if (!is_initialized_) {
    return;
  }
  Musin::AudioOutput::update(lowpass_);
}

void AudioEngine::play_on_voice(uint8_t voice_index, size_t sample_index, uint8_t velocity) {
  if (!is_initialized_ || voice_index >= NUM_VOICES) {
    return;
  }

  if (sample_index >= Musin::num_samples) {
      // TODO: Decide how to handle invalid sample index (e.g., log, play silence)
      return;
  }

  Voice &voice = voices_[voice_index];

  voice.reader->set_source(Musin::all_samples[sample_index].data,
                           Musin::all_samples[sample_index].length);

  const float gain = map_velocity_to_gain(velocity);
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

void AudioEngine::set_global_effect_parameter(uint8_t effect_id, uint8_t value) {
  if (!is_initialized_) {
    return;
  }

  switch (effect_id) {
  case EFFECT_ID_GLOBAL_FILTER_FREQ: {
    const float freq = map_value_to_freq(value);
    lowpass_.filter.frequency(freq);
    break;
  }
  case EFFECT_ID_GLOBAL_CRUSH_RATE: {
    const float rate = map_value_to_crush_rate(value);
    crusher_.sampleRate(rate);
    break;
  }
  // TODO: Add cases for other global effects (e.g., filter resonance, crush bits)
  default:
    break;
  }
}

void AudioEngine::set_voice_effect_parameter(uint8_t voice_index, uint8_t effect_id,
                                             uint8_t value) {
  if (!is_initialized_ || voice_index >= NUM_VOICES) {
    return;
  }

  switch (effect_id) {
  case EFFECT_ID_VOICE_VOLUME: {
    const float gain = map_velocity_to_gain(value);
    mixer_.gain(voice_index, gain);
    break;
  }
  // TODO: Add cases for other per-voice effects (e.g., panning)
  default:
    break;
  }
}

void AudioEngine::set_pitch(uint8_t voice_index, uint8_t value) {
  if (!is_initialized_ || voice_index >= NUM_VOICES) {
    return;
  }

  const float pitch_multiplier = map_pitch_value_to_multiplier(value);
  voices_[voice_index].current_pitch = pitch_multiplier;
  // TODO: Consider if pitch should affect currently playing sound (requires Sound modification)
}

} // namespace SB25

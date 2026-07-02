#include "audio_engine.h"

#include "config.h"
#include "musin/audio/audio_output.h"
#include "musin/hal/debug_utils.h"
#include "sample_repository.h"
#include "sample_slot_manager.h"

#include <algorithm>
#include <cmath>

extern "C" {
#include "hardware/sync.h"
}

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

// Runs in the I2S DMA interrupt: must stay RAM-resident (see AGENTS.md).
void __not_in_flash_func(AudioEngine::Voice::fill_buffer)(
    AudioBlock &out_samples) {
  sound.fill_buffer(out_samples);

  if (!decay_active) {
    return;
  }

  uint32_t position = frames_rendered;
  for (int16_t &sample : out_samples) {
    float gain = 0.0f;
    if (position < decay_end_frame) {
      gain = static_cast<float>(decay_end_frame - position) * decay_scale;
    }
    sample = static_cast<int16_t>(static_cast<float>(sample) * gain);
    ++position;
  }
  frames_rendered = position;
}

AudioEngine::AudioEngine(const SampleRepository &repository,
                         SampleSlotManager &slot_manager, musin::Logger &logger)
    : sample_repository_(repository), slot_manager_(slot_manager),
      logger_(logger),
      voice_sources_{&voices_[0], &voices_[1], &voices_[2], &voices_[3]},
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
  AudioOutput::attach_source(highpass_);
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
  AudioOutput::update();
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
    auto path_opt = sample_repository_.get_path(sample_index);
    if (path_opt.has_value() &&
        slot_manager_.request_load(voice_index, sample_index, *path_opt)) {
      // The voice may still be sounding from the buffer being overwritten;
      // keep the interrupt-driven render out while it is replaced.
      const uint32_t saved_irq = save_and_disable_interrupts();
      slot_manager_.commit_staging();
      restore_interrupts(saved_irq);
    }
    // On load failure the voice keeps its current sample.
  }

  if (slot_manager_.voice_length(voice_index) == 0) {
    return;
  }

  const float normalized_velocity = static_cast<float>(velocity) / 127.0f;
  const float gain = normalized_velocity * voice.current_gain;

  // Decay envelope: gain ramps linearly from full at the trigger to zero at
  // current_decay of the sample's playback duration, silencing the rest.
  // 1.0 disables the envelope. A floor keeps very low values click-free.
  constexpr uint32_t MIN_DECAY_FRAMES = 128;
  const float speed = std::max(voice.current_pitch, 0.2f);
  const uint32_t total_frames = static_cast<uint32_t>(
      static_cast<float>(slot_manager_.voice_length(voice_index)) / speed);
  const uint32_t decay_end_frame =
      std::max(static_cast<uint32_t>(static_cast<float>(total_frames) *
                                     voice.current_decay),
               MIN_DECAY_FRAMES);
  const bool decay_active = voice.current_decay < 1.0f;
  const float decay_scale =
      decay_active ? 1.0f / static_cast<float>(decay_end_frame) : 0.0f;

  // The render runs in the DMA interrupt; keep it from reading the voice
  // mid-retrigger.
  const uint32_t saved_irq = save_and_disable_interrupts();
  voice.reader.set_source(slot_manager_.voice_data(voice_index),
                          slot_manager_.voice_length(voice_index));
  mixer_.gain(voice_index, gain);
  voice.decay_active = decay_active;
  voice.decay_end_frame = decay_end_frame;
  voice.decay_scale = decay_scale;
  voice.frames_rendered = 0;
  voice.sound.play(voice.current_pitch);
  restore_interrupts(saved_irq);
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

void AudioEngine::set_track_gain(uint8_t voice_index, float value) {
  if (!is_initialized_ || voice_index >= NUM_VOICES) {
    return;
  }
  voices_[voice_index].current_gain = std::clamp(value, 0.0f, 1.0f);
}

void AudioEngine::set_track_decay(uint8_t voice_index, float value) {
  if (!is_initialized_ || voice_index >= NUM_VOICES) {
    return;
  }
  voices_[voice_index].current_decay = std::clamp(value, 0.0f, 1.0f);
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

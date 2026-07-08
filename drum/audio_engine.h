#ifndef DRUM_AUDIO_ENGINE_H_
#define DRUM_AUDIO_ENGINE_H_

#include "etl/array.h"
#include "etl/observer.h" // Required for etl::observer
#include "etl/optional.h"
#include "events.h" // Required for drum::Events::NoteEvent
#include <cstddef>
#include <cstdint>

#include "config.h"
#include "musin/audio/audio_input.h"
#include "musin/audio/buffer_source.h"
#include "musin/audio/crusher.h"
#include "musin/audio/filter.h"
#include "musin/audio/line_in_source.h"
#include "musin/audio/memory_reader.h"
#include "musin/audio/mixer.h"
#include "musin/audio/sound.h"
#include "musin/hal/logger.h"

namespace drum {

class SampleRepository;  // Forward declaration
class SampleSlotManager; // Forward declaration

constexpr size_t NUM_VOICES = 4;

/**
 * @brief Manages audio playback, mixing, and effects for the drum machine.
 */
class AudioEngine : public etl::observer<drum::Events::NoteEvent> {
private:
  /**
   * @brief Internal structure representing a single audio voice.
   *
   * Renders its Sound and applies an optional linear decay envelope that
   * fades the voice to silence over the tail of the sample. fill_buffer runs
   * in the I2S DMA interrupt and must stay RAM-resident.
   */
  struct Voice : BufferSource {
    musin::MemorySampleReader reader;
    Sound sound;
    float current_pitch = 1.0f;
    float current_gain = 1.0f;
    float current_decay = 1.0f; // Fraction of duration where gain reaches 0.

    // Decay envelope state, set at trigger time and consumed in the ISR.
    bool decay_active = false;
    uint32_t decay_end_frame = 0;
    float decay_scale = 0.0f; // 1 / decay_end_frame
    uint32_t frames_rendered = 0;

    Voice();

    void fill_buffer(AudioBlock &out_samples) override;
  };

public:
  AudioEngine(const SampleRepository &repository,
              SampleSlotManager &slot_manager, musin::Logger &logger);
  ~AudioEngine() = default;

  // Delete copy and move operations
  AudioEngine(const AudioEngine &) = delete;
  AudioEngine &operator=(const AudioEngine &) = delete;
  AudioEngine(AudioEngine &&) = delete;
  AudioEngine &operator=(AudioEngine &&) = delete;

  /**
   * @brief Initializes the audio engine and hardware.
   * Must be called before any other methods.
   * @return true on success, false otherwise.
   */
  bool init();

  /**
   * @brief Deinitializes the audio engine and puts codec into sleep mode.
   */
  void deinit();

  /**
   * @brief Periodically updates the audio output buffer.
   * This should be called frequently from the main application loop.
   */
  void process();

  /**
   * @brief Starts playback of a sample on a specific voice/track.
   * If the voice is already playing, it should be re-triggered.
   * @param voice_index The voice/track index (0 to NUM_VOICES - 1).
   * @param sample_index The index of the sample within the global sample bank.
   * @param velocity Playback velocity (0-127), affecting volume.
   */
  void play_on_voice(uint8_t voice_index, size_t sample_index,
                     uint8_t velocity);

  /**
   * @brief Stops playback on a specific voice/track immediately by setting
   * volume to 0.
   * @param voice_index The voice/track index (0 to NUM_VOICES - 1).
   */
  void stop_voice(uint8_t voice_index);

  /**
   * @brief Sets the pitch multiplier for a specific voice/track for the *next*
   * time it's triggered.
   * @param voice_index The voice/track index (0 to NUM_VOICES - 1).
   * @param value The pitch value, normalized (0.0f to 1.0f), mapped internally
   * to a multiplier.
   */
  void set_pitch(uint8_t voice_index, float value);

  /**
   * @brief Sets the gain for a specific voice/track for the *next* time it's
   * triggered. Scales the velocity-derived gain.
   * @param voice_index The voice/track index (0 to NUM_VOICES - 1).
   * @param value The gain value, normalized (0.0f to 1.0f).
   */
  void set_track_gain(uint8_t voice_index, float value);

  /**
   * @brief Sets the decay for a specific voice/track for the *next* time it's
   * triggered. Gain ramps linearly from full at the trigger to silence at
   * this fraction of the sample's playback duration; 1.0 disables the fade.
   * @param voice_index The voice/track index (0 to NUM_VOICES - 1).
   * @param value The decay value, normalized (0.0f to 1.0f).
   */
  void set_track_decay(uint8_t voice_index, float value);

  /**
   * @brief Sets the master output volume.
   * @param volume The desired volume level (0.0f to 1.0f).
   */
  void set_volume(float volume);

  /**
   * @brief Mutes the audio output.
   */
  void mute();

  /**
   * @brief Unmutes the audio output.
   */
  void unmute();

  /**
   * @brief Sets the global lowpass filter cutoff frequency.
   * @param normalized_value The frequency value, normalized (0.0f to 1.0f).
   */
  void set_filter_frequency(float normalized_value);

  /**
   * @brief Sets the global lowpass filter resonance.
   * @param normalized_value The resonance value, normalized (0.0f to 1.0f).
   */
  void set_filter_resonance(float normalized_value);

  /**
   * @brief Sets the global crusher sample rate reduction amount.
   * @param normalized_value The crush amount, normalized (0.0f to 1.0f).
   */
  void set_crush_rate(float normalized_value);

  /**
   * @brief Sets the global crusher bit depth.
   * @param normalized_value The desired depth amount, normalized (0.0f
   * to 1.0f).
   */
  void set_crush_depth(float normalized_value);

  /**
   * @brief Handles incoming NoteEvents to play or stop sounds.
   * @param event The NoteEvent received.
   */
  void notification(drum::Events::NoteEvent event) override;

  /**
   * @brief Applies a line input routing mode.
   *
   * Enables or disables the capture path as needed and gates the line input
   * into the mixer (PreFx) or onto the output stage (PostFx). Cheap when the
   * routing is unchanged, so it may be called every main-loop iteration with
   * the current setting value.
   */
  void set_line_in_routing(config::audio::LineInRouting routing);

private:
  static constexpr size_t LINE_IN_MIXER_CHANNEL = NUM_VOICES;

  const SampleRepository &sample_repository_;
  SampleSlotManager &slot_manager_;
  musin::Logger &logger_;
  etl::array<Voice, NUM_VOICES> voices_;
  etl::array<BufferSource *, NUM_VOICES + 1> voice_sources_;

  musin::audio::AudioInput audio_input_;
  musin::audio::LineInSource line_in_pre_;
  musin::audio::LineInSource line_in_post_;
  musin::audio::AudioMixer<NUM_VOICES + 1> mixer_;
  musin::audio::Crusher crusher_;
  musin::audio::Lowpass lowpass_;
  musin::audio::Highpass highpass_;
  musin::audio::AudioMixer<2> output_mixer_;
  config::audio::LineInRouting line_in_routing_ =
      config::audio::LineInRouting::Off;

  // profiler_ member is removed, ProfileSection enum remains for use with the
  // global profiler
  enum class ProfileSection {
    AUDIO_PROCESS_UPDATE,
    PLAY_ON_VOICE_UPDATE
  };

  bool is_initialized_ = false;
  bool muted_ = false;
  float current_volume_ = 1.0f;
};

} // namespace drum

#endif // DRUM_AUDIO_ENGINE_H_

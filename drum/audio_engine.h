#ifndef DRUM_AUDIO_ENGINE_H_
#define DRUM_AUDIO_ENGINE_H_

#include "etl/array.h"
#include "etl/observer.h" // Required for etl::observer
#include "etl/optional.h"
#include "events.h" // Required for drum::Events::NoteEvent
#include <cstddef>
#include <cstdint>

#include "musin/audio/attack_buffering_sample_reader.h" // Changed include
#include "musin/audio/buffer_source.h"
#include "musin/audio/crusher.h"
#include "musin/audio/filter.h"
#include "musin/audio/mixer.h"
#include "musin/audio/sound.h"
#include "musin/hal/logger.h"

namespace drum {

class SampleRepository; // Forward declaration

constexpr size_t NUM_VOICES = 4;

/**
 * @brief Manages audio playback, mixing, and effects for the drum machine.
 */
class AudioEngine : public etl::observer<drum::Events::NoteEvent> {
private:
  /**
   * @brief Internal structure representing a single audio voice.
   */
  struct Voice {
    etl::optional<musin::AttackBufferingSampleReader<>>
        reader; // Use default template arg
    Sound sound;
    float current_pitch = 1.0f;

    Voice();
  };

public:
  explicit AudioEngine(const SampleRepository &repository,
                       musin::Logger &logger);
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

private:
  const SampleRepository &sample_repository_;
  musin::Logger &logger_;
  etl::array<Voice, NUM_VOICES> voices_;
  etl::array<BufferSource *, NUM_VOICES> voice_sources_;

  musin::audio::AudioMixer<NUM_VOICES> mixer_;
  musin::audio::Crusher crusher_;
  musin::audio::Lowpass lowpass_;
  musin::audio::Highpass highpass_;

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

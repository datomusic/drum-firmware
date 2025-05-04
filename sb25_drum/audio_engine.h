#ifndef SB25_DRUM_AUDIO_ENGINE_H_
#define SB25_DRUM_AUDIO_ENGINE_H_

#include "etl/array.h"
#include "etl/optional.h"
#include <cstddef> // For size_t
#include <cstdint>

// Include necessary Musin headers instead of forward declaring classes used by value
#include "musin/audio/crusher.h"
#include "musin/audio/filter.h" // Provides Lowpass
#include "musin/audio/memory_reader.h"
#include "musin/audio/mixer.h"
#include "musin/audio/sound.h"

// Forward declaration is sufficient for pointer usage
namespace Musin {
class BufferSource;
} // namespace Musin

namespace SB25 {

// Define constants for clarity
constexpr size_t NUM_VOICES = 4;
constexpr uint8_t EFFECT_ID_VOICE_VOLUME = 0;

/**
 * @brief Manages audio playback, mixing, and effects for the drum machine.
 */
class AudioEngine {
private:
  /**
   * @brief Internal structure representing a single audio voice.
   */
  struct Voice {
    etl::optional<Musin::MemorySampleReader> reader;
    Sound sound;
    float current_pitch = 1.0f;

    Voice();
  };

public:
  AudioEngine();
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
  void play_on_voice(uint8_t voice_index, size_t sample_index, uint8_t velocity);

  /**
   * @brief Stops playback on a specific voice/track immediately by setting volume to 0.
   * @param voice_index The voice/track index (0 to NUM_VOICES - 1).
   */
  void stop_voice(uint8_t voice_index);

  /**
   * @brief Sets a per-voice/track effect parameter.
   * @param voice_index The voice/track index (0 to NUM_VOICES - 1).
   * @param effect_id Identifier for the per-voice effect (e.g., EFFECT_ID_VOICE_...).
   * @param value The parameter value, normalized (0.0f to 1.0f).
   */
  void set_voice_effect_parameter(uint8_t voice_index, uint8_t effect_id, float value);

  /**
   * @brief Sets the pitch multiplier for a specific voice/track for the *next* time it's triggered.
   * @param voice_index The voice/track index (0 to NUM_VOICES - 1).
   * @param value The pitch value, normalized (0.0f to 1.0f), mapped internally to a multiplier.
   */
  void set_pitch(uint8_t voice_index, float value);

  /**
   * @brief Sets the master output volume.
   * @param volume The desired volume level (0.0f to 1.0f).
   */
  void set_volume(float volume);

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
   * @param depth The desired bit depth (e.g., 1 to 16). Values outside a reasonable range might be clamped internally by the Crusher effect.
   */
  void set_crush_depth(uint8_t depth);
 
private:
  etl::array<Voice, NUM_VOICES> voices_;
  etl::array<BufferSource *, NUM_VOICES> voice_sources_;

  AudioMixer<NUM_VOICES> mixer_;
  Crusher crusher_;
  Lowpass lowpass_;

  bool is_initialized_ = false;
};

} // namespace SB25

#endif // SB25_DRUM_AUDIO_ENGINE_H_

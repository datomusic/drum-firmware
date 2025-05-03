#include "etl/array.h"
#include "etl/optional.h"
#include <cstdint>
#include <cstddef> // For size_t

// Forward declarations for Musin components
namespace Musin {
class BufferSource;
template <size_t N> class AudioMixer;
class Crusher;
class Lowpass;
class Sound;
class MemorySampleReader;
} // namespace Musin

namespace SB25 {

// Define constants for clarity
constexpr size_t NUM_VOICES = 4;
constexpr uint8_t EFFECT_ID_GLOBAL_FILTER_FREQ = 0;
constexpr uint8_t EFFECT_ID_GLOBAL_CRUSH_RATE = 1;
constexpr uint8_t EFFECT_ID_VOICE_VOLUME = 0;
// TODO: Add more effect IDs as needed

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
    Musin::Sound sound;
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
   * @brief Sets a global effect parameter.
   * @param effect_id Identifier for the global effect (e.g., EFFECT_ID_GLOBAL_...).
   * @param value The parameter value (typically 0-127).
   */
  void set_global_effect_parameter(uint8_t effect_id, uint8_t value);

  /**
   * @brief Sets a per-voice/track effect parameter.
   * @param voice_index The voice/track index (0 to NUM_VOICES - 1).
   * @param effect_id Identifier for the per-voice effect (e.g., EFFECT_ID_VOICE_...).
   * @param value The parameter value (typically 0-127).
   */
  void set_voice_effect_parameter(uint8_t voice_index, uint8_t effect_id, uint8_t value);

  /**
   * @brief Sets the pitch for a specific voice/track for the *next* time it's triggered.
   * @param voice_index The voice/track index (0 to NUM_VOICES - 1).
   * @param value The pitch value (typically 0-127, mapped internally).
   */
  void set_pitch(uint8_t voice_index, uint8_t value);

private:
  etl::array<Voice, NUM_VOICES> voices_;
  etl::array<Musin::BufferSource *, NUM_VOICES> voice_sources_;

  Musin::AudioMixer<NUM_VOICES> mixer_;
  Musin::Crusher crusher_;
  Musin::Lowpass lowpass_;

  bool is_initialized_ = false;
};

} // namespace SB25

#endif // SB25_DRUM_AUDIO_ENGINE_H_

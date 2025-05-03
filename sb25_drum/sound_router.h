#ifndef SB25_DRUM_SOUND_ROUTER_H_
#define SB25_DRUM_SOUND_ROUTER_H_

#include "audio_engine.h"
#include "events.h" // Include NoteEvent definition
#include "etl/observer.h"
#include <array>
#include <cstdint>
#include <optional>

namespace SB25 {

/**
 * @brief Defines the possible output destinations for sound events.
 */
enum class OutputMode : uint8_t {
  MIDI,
  AUDIO,
  BOTH
};

/**
 * @brief Defines logical identifiers for controllable parameters/effects.
 * These abstract away the specific MIDI CC numbers or internal audio engine parameters.
 */
enum class ParameterID : uint8_t {
  // Per Voice/Track Parameters (Mapped from DRUM 1-4, PITCH 1-4 knobs)
  DRUM_PARAM_1, // Example: Decay, Tone, etc. for Track 1
  DRUM_PARAM_2, // Example: Decay, Tone, etc. for Track 2
  DRUM_PARAM_3, // Example: Decay, Tone, etc. for Track 3
  DRUM_PARAM_4, // Example: Decay, Tone, etc. for Track 4
  PITCH,        // Pitch control for a specific track

  // Global Parameters (Mapped from other knobs)
  FILTER_CUTOFF,
  VOLUME,
  CRUSH_AMOUNT,
  // Note: RANDOM, SWING, REPEAT, SPEED are handled directly by SequencerController/InternalClock
};

/*
 * @brief Routes sound trigger events, parameter changes, and NoteEvents to MIDI, internal audio, or both.
 */
class SoundRouter : public etl::observer<SB25::Events::NoteEvent> {
public:
  /**
   * @brief Constructor.
   * @param audio_engine Reference to the audio engine instance.
   */
  explicit SoundRouter(AudioEngine &audio_engine);

  // Delete copy and move operations
  SoundRouter(const SoundRouter &) = delete;
  SoundRouter &operator=(const SoundRouter &) = delete;
  SoundRouter(SoundRouter &&) = delete;
  SoundRouter &operator=(SoundRouter &&) = delete;

  /**
   * @brief Sets the current output mode (MIDI, AUDIO, or BOTH).
   * @param mode The desired output mode.
   */
  void set_output_mode(OutputMode mode);

  /**
   * @brief Gets the current output mode.
   * @return The current OutputMode.
   */
  [[nodiscard]] OutputMode get_output_mode() const;

  /**
   * @brief Triggers a sound event (note on/off) for a specific track.
   * Routes the event based on the current output mode.
   * @param track_index The logical track index (0-3).
   * @param midi_note The MIDI note number associated with the track.
   * @param velocity The velocity (0-127). Velocity 0 signifies note off.
   */
  void trigger_sound(uint8_t track_index, uint8_t midi_note, uint8_t velocity);

  /**
   * @brief Sets the value for a specific controllable parameter.
   * Routes the parameter change based on the current output mode.
   * @param param_id The logical identifier of the parameter.
   * @param track_index Optional track index (0-3) if the parameter is per-track (e.g., PITCH).
   * @param value The parameter value (typically 0-127).
   */
  void set_parameter(ParameterID param_id, std::optional<uint8_t> track_index, uint8_t value);

  /**
   * @brief Handles incoming NoteEvents.
   * @param event The NoteEvent received.
   */
  void notification(SB25::Events::NoteEvent event) override;

  // TODO: Add method to update the track_index -> sample_id mapping if needed.

private:
  AudioEngine &_audio_engine;
  OutputMode _output_mode;
  // TODO: Add mapping from track_index to sample_id if AudioEngine needs it.
  // std::array<uint32_t, 4> _track_sample_map;
};

} // namespace SB25

#endif // SB25_DRUM_SOUND_ROUTER_H_

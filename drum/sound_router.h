#ifndef SB25_DRUM_SOUND_ROUTER_H_
#define SB25_DRUM_SOUND_ROUTER_H_

#include "audio_engine.h"
#include "config.h" // For NUM_TRACKS, NUM_STEPS_PER_TRACK and potentially sound_router::MAX_NOTE_EVENT_OBSERVERS
#include "etl/observer.h"
#include "events.h" // Include NoteEvent definition
#include <array>
#include <cstdint>
#include <optional>

namespace drum {

// Forward declaration
template <size_t NumTracks, size_t NumSteps> class SequencerController;

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
enum class Parameter : uint8_t {
  // Per Voice/Track Parameters (Mapped from DRUM 1-4, PITCH 1-4 knobs)
  DRUM_PRESSURE_1, // Example: Decay, Tone, etc. for Track 1
  DRUM_PRESSURE_2, // Example: Decay, Tone, etc. for Track 2
  DRUM_PRESSURE_3, // Example: Decay, Tone, etc. for Track 3
  DRUM_PRESSURE_4, // Example: Decay, Tone, etc. for Track 4
  PITCH,           // Pitch control for a specific track

  // Global Parameters (Mapped from other knobs)
  FILTER_FREQUENCY,
  FILTER_RESONANCE,
  VOLUME,
  CRUSH_RATE,
  CRUSH_DEPTH,
  // Note: SPEED is handled directly by SequencerController/InternalClock.
  // SWING, TEMPO_BPM, RANDOM_EFFECT, REPEAT_EFFECT are handled via set_parameter.
  SWING,
  TEMPO_BPM,
  RANDOM_EFFECT,
  REPEAT_EFFECT,
};

/*
 * @brief Routes sound trigger events, parameter changes, and NoteEvents to MIDI, internal audio, or
 * both.
 */
class SoundRouter : public etl::observer<drum::Events::NoteEvent>,
                    public etl::observable<etl::observer<drum::Events::NoteEvent>,
                                           drum::config::sound_router::MAX_NOTE_EVENT_OBSERVERS> {
public:
  /**
   * @brief Constructor.
   * @param audio_engine Reference to the audio engine instance.
   * @param sequencer_controller Reference to the sequencer controller instance.
   */
  explicit SoundRouter(
      AudioEngine &audio_engine,
      SequencerController<config::NUM_TRACKS, config::NUM_STEPS_PER_TRACK> &sequencer_controller);

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
   * @param value The parameter value, typically normalized between 0.0f and 1.0f.
   * @param track_index Optional track index (0-3) if the parameter is per-track (e.g., PITCH).
   *                    Defaults to std::nullopt if not provided.
   */
  void set_parameter(Parameter param_id, float value,
                     std::optional<uint8_t> track_index = std::nullopt);

  /**
   * @brief Handles incoming NoteEvents.
   * @param event The NoteEvent received.
   */
  void notification(drum::Events::NoteEvent event) override;

  /**
   * @brief Handles an incoming MIDI Note On/Off message.
   * If the note corresponds to a configured track:
   * - For Note On (velocity > 0): Plays the sound on the audio engine and sets the active note
   *   for that track in the sequencer controller.
   * - For Note Off (velocity == 0): Plays the sound on the audio engine (which should handle
   *   velocity 0 as silence or note off).
   * @param note The MIDI note number.
   * @param velocity The MIDI velocity (0 for Note Off).
   */
  void handle_incoming_midi_note(uint8_t note, uint8_t velocity);

private:
  AudioEngine &_audio_engine;
  SequencerController<config::NUM_TRACKS, config::NUM_STEPS_PER_TRACK> &_sequencer_controller;
  OutputMode _output_mode;
};

} // namespace drum

#endif // SB25_DRUM_SOUND_ROUTER_H_

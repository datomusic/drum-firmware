#ifndef SB25_DRUM_MESSAGE_ROUTER_H_
#define SB25_DRUM_MESSAGE_ROUTER_H_

#include "audio_engine.h"
#include "config.h" // For NUM_TRACKS, NUM_STEPS_PER_TRACK and potentially message_router::MAX_NOTE_EVENT_OBSERVERS
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
 * @brief Defines whether panel controls directly affect parameters or if MIDI has control.
 */
enum class LocalControlMode : uint8_t {
  ON, // Panel controls affect parameters; MIDI CCs for these parameters might be ignored or have
      // different behavior.
  OFF // Panel controls might only send MIDI; MIDI CCs primarily control parameters.
};

/**
 * @brief Defines logical identifiers for controllable parameters/effects.
 * These abstract away the specific MIDI CC numbers or internal audio engine parameters.
 */
enum class Parameter : uint8_t {
  // Per-Track Parameters
  PITCH, // Pitch control for a specific track (CC 21-24)

  // Global Parameters
  VOLUME,           // CC 7
  SWING,            // CC 9
  CRUSH_EFFECT,     // CC 12
  TEMPO,            // CC 15
  RANDOM_EFFECT,    // CC 16
  REPEAT_EFFECT,    // CC 17
  FILTER_FREQUENCY, // CC 74
  FILTER_RESONANCE, // CC 75
};

/*
 * @brief Routes sound trigger events, parameter changes, and NoteEvents to MIDI, internal audio, or
 * both.
 */
class MessageRouter : public etl::observer<drum::Events::SysExTransferStateChangeEvent>,
                      public etl::observable<etl::observer<drum::Events::NoteEvent>,
                                             drum::config::MAX_NOTE_EVENT_OBSERVERS> {
public:
  /**
   * @brief Constructor.
   * @param audio_engine Reference to the audio engine instance.
   * @param sequencer_controller Reference to the sequencer controller instance.
   */
  explicit MessageRouter(
      AudioEngine &audio_engine,
      SequencerController<config::NUM_TRACKS, config::NUM_STEPS_PER_TRACK> &sequencer_controller);

  // Delete copy and move operations
  MessageRouter(const MessageRouter &) = delete;
  MessageRouter &operator=(const MessageRouter &) = delete;
  MessageRouter(MessageRouter &&) = delete;
  MessageRouter &operator=(MessageRouter &&) = delete;

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
   * @brief Sets the local control mode.
   * @param mode The desired local control mode.
   */
  void set_local_control_mode(LocalControlMode mode);

  /**
   * @brief Gets the current local control mode.
   * @return The current LocalControlMode.
   */
  [[nodiscard]] LocalControlMode get_local_control_mode() const;
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
   * @brief Processes events from the note event queue.
   * This should be called from the main loop.
   */
  void update();

  /**
   * @brief Notification handler for SysEx transfer state changes.
   * @param event The event indicating the transfer state.
   */
  void notification(drum::Events::SysExTransferStateChangeEvent event);

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

  /**
   * @brief Handles an incoming MIDI Control Change message.
   * This method will map the CC number to a `drum::Parameter` and apply the change.
   * @param controller The MIDI CC number.
   * @param value The MIDI CC value.
   */
  void handle_incoming_midi_cc(uint8_t controller, uint8_t value);

private:
  AudioEngine &_audio_engine;
  SequencerController<config::NUM_TRACKS, config::NUM_STEPS_PER_TRACK> &_sequencer_controller;
  OutputMode _output_mode;
  LocalControlMode _local_control_mode;
  std::optional<LocalControlMode> _previous_local_control_mode;
};

} // namespace drum

#endif // SB25_DRUM_MESSAGE_ROUTER_H_

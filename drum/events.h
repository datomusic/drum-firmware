#ifndef SB25_DRUM_EVENTS_H_
#define SB25_DRUM_EVENTS_H_

#include <cstdint>
#include <optional> // Required for std::optional

namespace drum {
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
} // namespace drum

namespace drum::Events {

/**
 * @brief Event data structure for note on/off events.
 */
struct NoteEvent {
  uint8_t track_index; // Logical track index (0-3)
  uint8_t note;        // MIDI note number
  uint8_t velocity;    // MIDI velocity (0-127, 0 means note off)
};

/**
 * @brief Event data structure for parameter change events.
 */
struct ParameterChangeEvent {
  drum::Parameter param_id;           // The parameter that changed
  float value;                        // The new value (typically 0.0f to 1.0f)
  std::optional<uint8_t> track_index; // Optional track index for per-track parameters
};

/**
 * @brief Event structure for SysEx file transfer state changes.
 */
struct SysExTransferStateChangeEvent {
  bool is_active; // true when transfer starts, false when it ends
};

} // namespace drum::Events

#endif // SB25_DRUM_EVENTS__H_

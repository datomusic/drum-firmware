#ifndef SB25_DRUM_EVENTS_H_
#define SB25_DRUM_EVENTS_H_

#include <cstdint>
#include <optional> // Required for std::optional

// Forward declare Parameter enum to avoid circular dependency if Parameter is in sound_router.h
// Alternatively, Parameter could be moved to this file or a common types file.
namespace drum {
enum class Parameter : uint8_t;
}

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
  drum::Parameter param_id;        // The parameter that changed
  float value;               // The new value (typically 0.0f to 1.0f)
  std::optional<uint8_t> track_index; // Optional track index for per-track parameters
};

} // namespace drum::Events

#endif // SB25_DRUM_EVENTS__H_

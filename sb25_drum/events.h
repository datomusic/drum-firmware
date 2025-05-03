#ifndef SB25_DRUM_EVENTS_H_
#define SB25_DRUM_EVENTS_H_

#include <cstdint>

namespace SB25::Events {

/**
 * @brief Event data structure for note on/off events.
 */
struct NoteEvent {
  uint8_t track_index; // Logical track index (0-3)
  uint8_t note;        // MIDI note number
  uint8_t velocity;    // MIDI velocity (0-127, 0 means note off)
};

} // namespace SB25::Events

#endif // SB25_DRUM_EVENTS__H_

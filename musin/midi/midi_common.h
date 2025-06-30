#ifndef MUSIN_MIDI_MIDI_COMMON_H
#define MUSIN_MIDI_MIDI_COMMON_H

#include "midi_Defs.h"
#include <cstdint>

namespace musin::midi {

struct NoteMessageData {
  uint8_t channel;
  uint8_t note;
  uint8_t velocity;
};

struct ControlChangeData {
  uint8_t channel;
  uint8_t controller;
  uint8_t value;
};

struct PitchBendData {
  uint8_t channel;
  int bend_value;
};

struct SystemRealtimeData {
  ::midi::MidiType type;
};

} // namespace musin::midi

#endif // MUSIN_MIDI_MIDI_COMMON_H
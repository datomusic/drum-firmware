#ifndef MIDI_H_Z6SY8IRY
#define MIDI_H_Z6SY8IRY
#include <midi_Defs.h>

namespace MIDI {
using MidiType = ::midi::MidiType; // Alias the original library's MidiType

typedef void(VoidCallback)();
typedef void(SyxCallback)(uint8_t *data, unsigned length);
typedef void(NoteCallback)(uint8_t channel, uint8_t note, uint8_t velocity);
typedef void(ControlChangeCallback)(uint8_t channel, uint8_t controller, uint8_t value);
typedef void(PitchBendCallback)(uint8_t channel, int bend);

struct Callbacks {
  NoteCallback *note_on = nullptr;
  NoteCallback *note_off = nullptr;
  VoidCallback *clock = nullptr;
  VoidCallback *start = nullptr;
  VoidCallback *cont = nullptr;
  VoidCallback *stop = nullptr;
  ControlChangeCallback *cc = nullptr;
  PitchBendCallback *pitch_bend = nullptr;
  SyxCallback *sysex = nullptr;
};

void init(const Callbacks &callbacks);
/** @brief Read MIDI messages for a specific channel. */
void read(uint8_t channel);
/** @brief Read MIDI messages for all channels (OMNI). */
void read();
void sendRealTime(MidiType message);
void sendControlChange(uint8_t cc, uint8_t value, uint8_t channel);
void sendNoteOn(uint8_t inNoteNumber, uint8_t inVelocity, uint8_t inChannel);
void sendNoteOff(uint8_t inNoteNumber, uint8_t inVelocity, uint8_t inChannel);
void sendPitchBend(int bend, uint8_t channel);
void sendSysEx(unsigned length, const uint8_t *bytes);

}; // namespace MIDI

#endif /* end of include guard: MIDI_H_Z6SY8IRY */

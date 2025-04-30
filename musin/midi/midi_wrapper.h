#ifndef MIDI_H_Z6SY8IRY
#define MIDI_H_Z6SY8IRY
#include <midi_Defs.h>

namespace MIDI {
using MidiType = ::midi::MidiType; // Alias the original library's MidiType

typedef void(VoidCallback)();
typedef void(SyxCallback)(byte *data, unsigned length);
typedef void(NoteCallback)(byte channel, byte note, byte velocity);
typedef void(ControlChangeCallback)(byte channel, byte controller,
                                    byte value);
typedef void(PitchBendCallback)(byte channel, int bend);

struct Callbacks {
  NoteCallback *note_on;
  NoteCallback *note_off;
  VoidCallback *clock;
  VoidCallback *start;
  VoidCallback *cont;
  VoidCallback *stop;
  ControlChangeCallback *cc;
  PitchBendCallback *pitch_bend;
  SyxCallback *sysex;
};

void init(const Callbacks &callbacks);
/** @brief Read MIDI messages for a specific channel. */
void read(byte channel);
/** @brief Read MIDI messages for all channels (OMNI). */
void read();
void sendRealTime(MidiType message);
void sendControlChange(byte cc, byte value, byte channel);
void sendNoteOn(byte inNoteNumber, byte inVelocity, byte inChannel);
void sendNoteOff(byte inNoteNumber, byte inVelocity, byte inChannel);
void sendPitchBend(int bend, byte channel);
void sendSysEx(unsigned length, const byte *bytes);

}; // namespace MIDI

#endif /* end of include guard: MIDI_H_Z6SY8IRY */

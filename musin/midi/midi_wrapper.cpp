#include "midi_wrapper.h"
#include "musin/hal/UART.h"
#include <MIDI.h>
#include <USB-MIDI.h>

static USBMIDI_NAMESPACE::usbMidiTransport usbTransport(0);
static MIDI_NAMESPACE::MidiInterface<USBMIDI_NAMESPACE::usbMidiTransport> usb_midi(usbTransport);

static Musin::HAL::UART serial;
static MIDI_NAMESPACE::SerialMIDI<Musin::HAL::UART> serialTransport(serial);
static MIDI_NAMESPACE::MidiInterface<MIDI_NAMESPACE::SerialMIDI<Musin::HAL::UART>>
    serial_midi(serialTransport);

#define ALL_TRANSPORTS(function_call)                                                              \
  usb_midi.function_call;                                                                          \
  serial_midi.function_call;

void MIDI::init(const Callbacks &callbacks) {
  // Initialize in OMNI mode to listen on all channels
  ALL_TRANSPORTS(begin(MIDI_CHANNEL_OMNI));
  ALL_TRANSPORTS(setHandleClock(callbacks.clock));
  ALL_TRANSPORTS(setHandleNoteOn(callbacks.note_on));
  ALL_TRANSPORTS(setHandleNoteOff(callbacks.note_off));
  ALL_TRANSPORTS(setHandleNoteOff(callbacks.note_off));
  ALL_TRANSPORTS(setHandleStart(callbacks.start));
  ALL_TRANSPORTS(setHandleStop(callbacks.stop));
  ALL_TRANSPORTS(setHandleContinue(callbacks.cont));
  ALL_TRANSPORTS(setHandleControlChange(callbacks.cc));
  ALL_TRANSPORTS(setHandlePitchBend(callbacks.pitch_bend)); // Register pitch bend handler
  ALL_TRANSPORTS(setHandleSystemExclusive(callbacks.sysex));
}

void MIDI::read(const byte channel) {
  ALL_TRANSPORTS(read(channel));
}

// Overload for reading all channels (OMNI)
void MIDI::read() {
  ALL_TRANSPORTS(read());
}

void MIDI::sendRealTime(const midi::MidiType message) {
  ALL_TRANSPORTS(sendRealTime(message));
}

void MIDI::sendControlChange(const byte cc, const byte value, const byte channel) {
  ALL_TRANSPORTS(sendControlChange(cc, value, channel));
}

void MIDI::sendNoteOn(const byte note, const byte velocity, const byte channel) {
  ALL_TRANSPORTS(sendNoteOn(note, velocity, channel));
}

void MIDI::sendNoteOff(const byte note, const byte velocity, const byte channel) {
  ALL_TRANSPORTS(sendNoteOff(note, velocity, channel));
}

void MIDI::sendPitchBend(const int bend, const byte channel) {
  ALL_TRANSPORTS(sendPitchBend(bend, channel));
}

void MIDI::sendSysEx(const unsigned length, const byte *bytes) {
  ALL_TRANSPORTS(sendSysEx(length, bytes));
}

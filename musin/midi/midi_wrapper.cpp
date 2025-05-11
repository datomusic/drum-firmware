#include "midi_wrapper.h"
#include "../pico_uart.h"
#include <MIDI.h>
#include <USB-MIDI.h>

struct MIDISettings {
  /*! Running status enables short messages when sending multiple values
  of the same type and channel.\n
  Must be disabled to send USB MIDI messages to a computer
  Warning: does not work with some hardware, enable with caution.
  */
  static const bool UseRunningStatus = false;

  /*! NoteOn with 0 velocity should be handled as NoteOf.\n
  Set to true  to get NoteOff events when receiving null-velocity NoteOn messages.\n
  Set to false to get NoteOn  events when receiving null-velocity NoteOn messages.
  */
  static const bool HandleNullVelocityNoteOnAsNoteOff = true;

  /*! Setting this to true will make MIDI.read parse only one byte of data for each
  call when data is available. This can speed up your application if receiving
  a lot of traffic, but might induce MIDI Thru and treatment latency.
  */
  static const bool Use1ByteParsing = true;

  /*! Maximum size of SysEx receivable. Decrease to save RAM if you don't expect
  to receive SysEx, or adjust accordingly.
  */
  static const unsigned SysExMaxSize = 128;

  /*! Global switch to turn on/off sender ActiveSensing
  Set to true to send ActiveSensing
  Set to false will not send ActiveSensing message (will also save memory)
  */
  static const bool UseSenderActiveSensing = false;

  /*! Global switch to turn on/off receiver ActiveSensing
  Set to true to check for message timeouts (via ErrorCallback)
  Set to false will not check if chained device are still alive (if they use ActiveSensing) (will
  also save memory)
  */
  static const bool UseReceiverActiveSensing = false;

  /*! Active Sensing is intended to be sent
  repeatedly by the sender to tell the receiver that a connection is alive. Use
  of this message is optional. When initially received, the
  receiver will expect to receive another Active Sensing
  message each 300ms (max), and if it does not then it will
  assume that the connection has been terminated. At
  termination, the receiver will turn off all voices and return to
  normal (non- active sensing) operation.

  Typical value is 250 (ms) - an Active Sensing command is send every 250ms.
  (All Roland devices send Active Sensing every 250ms)

  Setting this field to 0 will disable sending MIDI active sensing.
  */
  static const uint16_t SenderActiveSensingPeriodicity = 0;
};

static usbMidi::usbMidiTransport usbTransport(0);
static midi::MidiInterface<usbMidi::usbMidiTransport, MIDISettings> usb_midi(usbTransport);

static PicoUART serial;
static midi::SerialMIDI<PicoUART> serialTransport(serial);
static midi::MidiInterface<midi::SerialMIDI<PicoUART>, MIDISettings> serial_midi(serialTransport);

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

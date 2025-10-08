#include "musin/midi/midi_wrapper.h"
#include "musin/boards/dato_submarine.h"
#include "musin/hal/UART.h"
#include "musin/hal/null_logger.h"
#include "musin/hal/pico_logger.h"
#include "musin/midi/midi_output_queue.h"
#include <MIDI.h>
#include <USB-MIDI.h>
#include <stdio.h> // For printf

#ifdef VERBOSE
static musin::PicoLogger midi_send_logger;
#else
static musin::NullLogger midi_send_logger;
#endif

struct MIDISettings {
  /*! Running status enables short messages when sending multiple values
  of the same type and channel.\n
  Must be disabled to send USB MIDI messages to a computer
  Warning: does not work with some hardware, enable with caution.
  */
  static const bool UseRunningStatus = false;

  /*! NoteOn with 0 velocity should be handled as NoteOf.\n
  Set to true  to get NoteOff events when receiving null-velocity NoteOn
  messages.\n Set to false to get NoteOn  events when receiving null-velocity
  NoteOn messages.
  */
  static const bool HandleNullVelocityNoteOnAsNoteOff = true;

  /*! Setting this to true will make MIDI.read parse only one byte of data for
  each call when data is available. This can speed up your application if
  receiving a lot of traffic, but might induce MIDI Thru and treatment latency.
  */
  static const bool Use1ByteParsing = true;

  /*! Maximum size of SysEx receivable. Decrease to save RAM if you don't expect
  to receive SysEx, or adjust accordingly.
  */
  static const unsigned SysExMaxSize = MIDI::SysExMaxSize;

  /*! When receiving a SysEx message, the library will wait for this amount of
  milliseconds before throwing a timeout error. This prevents the library
  from getting stuck waiting for an EOX byte that is never received.
  */
  static const unsigned SysExTimeOut = 1000; // 1 second

  /*! Global switch to turn on/off sender ActiveSensing
  Set to true to send ActiveSensing
  Set to false will not send ActiveSensing message (will also save memory)
  */
  static const bool UseSenderActiveSensing = false;

  /*! Global switch to turn on/off receiver ActiveSensing
  Set to true to check for message timeouts (via ErrorCallback)
  Set to false will not check if chained device are still alive (if they use
  ActiveSensing) (will also save memory)
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
static midi::MidiInterface<usbMidi::usbMidiTransport, MIDISettings>
    usb_midi(usbTransport);

using MidiUart =
    musin::hal::UART<DATO_SUBMARINE_MIDI_TX_PIN, DATO_SUBMARINE_MIDI_RX_PIN>;
static MidiUart midi_uart;
static midi::SerialMIDI<MidiUart> serialTransport(midi_uart);
static midi::MidiInterface<midi::SerialMIDI<MidiUart>, MIDISettings>
    serial_midi(serialTransport);

#define ALL_TRANSPORTS(function_call)                                          \
  usb_midi.function_call;                                                      \
  serial_midi.function_call;

void MIDI::init(const Callbacks &callbacks) {
  midi_uart.begin(31250); // Standard MIDI baud
  ALL_TRANSPORTS(begin(MIDI_CHANNEL_OMNI));
  ALL_TRANSPORTS(setHandleClock(callbacks.clock));
  ALL_TRANSPORTS(setHandleNoteOn(callbacks.note_on));
  ALL_TRANSPORTS(setHandleNoteOff(callbacks.note_off));
  ALL_TRANSPORTS(setHandleStart(callbacks.start));
  ALL_TRANSPORTS(setHandleStop(callbacks.stop));
  ALL_TRANSPORTS(setHandleContinue(callbacks.cont));
  ALL_TRANSPORTS(setHandleControlChange(callbacks.cc));
  ALL_TRANSPORTS(
      setHandlePitchBend(callbacks.pitch_bend)); // Register pitch bend handler
  ALL_TRANSPORTS(setHandleSystemExclusive(callbacks.sysex));
}

void MIDI::read(const byte channel) {
  ALL_TRANSPORTS(read(channel));
}

// Overload for reading all channels (OMNI)
void MIDI::read() {
  ALL_TRANSPORTS(read());
}

// --- Public Send Functions (Enqueue Messages) ---

void MIDI::sendRealTime(const midi::MidiType message) {
  musin::midi::OutgoingMidiMessage msg(message);
  musin::midi::enqueue_midi_message(msg, midi_send_logger);
}

void MIDI::sendControlChange(const byte cc, const byte value,
                             const byte channel) {
  musin::midi::OutgoingMidiMessage msg(channel, cc, value);
  musin::midi::enqueue_midi_message(msg, midi_send_logger);
}

void MIDI::sendNoteOn(const byte note, const byte velocity,
                      const byte channel) {
  musin::midi::OutgoingMidiMessage msg(channel, note, velocity, true);
  bool enqueued = musin::midi::enqueue_midi_message(msg, midi_send_logger);
  if (!enqueued) {
  }
}

void MIDI::sendNoteOff(const byte note, const byte velocity,
                       const byte channel) {
  musin::midi::OutgoingMidiMessage msg(channel, note, velocity, false);
  musin::midi::enqueue_midi_message(msg, midi_send_logger);
}

void MIDI::sendPitchBend(const int bend, const byte channel) {
  musin::midi::OutgoingMidiMessage msg(channel, bend);
  musin::midi::enqueue_midi_message(msg, midi_send_logger);
}

void MIDI::sendSysEx(const unsigned length, const byte *bytes) {
  // printf("Enqueuing SysEx message (%u bytes): ", length);
  // for (unsigned i = 0; i < length; ++i) {
  //   printf("%02X ", bytes[i]);
  // }
  // printf("\n");
  musin::midi::OutgoingMidiMessage msg(bytes, length);
  musin::midi::enqueue_midi_message(msg, midi_send_logger);
}

// --- Internal Actual Send Functions (Called by Queue Processor) ---

void MIDI::internal::_sendRealTime_actual(const midi::MidiType message) {
  // USB MIDI always non-blocking
  usb_midi.sendRealTime(message);

  // DIN MIDI: use non-blocking write to prevent jitter
  // Real-time messages are single-byte, so we can write directly
  // If FIFO is full, skip this clock tick rather than block (acceptable for
  // real-time messages as the next tick will arrive soon)
  midi_uart.write_nonblocking(static_cast<byte>(message));
}

void MIDI::internal::_sendControlChange_actual(const byte channel,
                                               const byte controller,
                                               const byte value) {
  ALL_TRANSPORTS(sendControlChange(controller, value, channel));
}

void MIDI::internal::_sendNoteOn_actual(const byte channel, const byte note,
                                        const byte velocity) {
  ALL_TRANSPORTS(sendNoteOn(note, velocity, channel));
}

void MIDI::internal::_sendNoteOff_actual(const byte channel, const byte note,
                                         const byte velocity) {
  ALL_TRANSPORTS(sendNoteOff(note, velocity, channel));
}

void MIDI::internal::_sendPitchBend_actual(const byte channel, const int bend) {
  ALL_TRANSPORTS(sendPitchBend(bend, channel));
}

void MIDI::internal::_sendSysEx_actual(const unsigned length,
                                       const byte *bytes) {
  // The underlying Arduino MIDI library adds the F0/F7 terminators itself.
  // We must pass only the payload.
  // This wrapper function will strip the terminators if they are present.
  if (length >= 2 && bytes[0] == 0xF0 && bytes[length - 1] == 0xF7) {
    ALL_TRANSPORTS(sendSysEx(length - 2, bytes + 1));
  } else {
    // If the message is not framed, send it as-is.
    ALL_TRANSPORTS(sendSysEx(length, bytes));
  }
}

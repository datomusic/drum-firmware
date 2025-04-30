#include "midi.h"

extern "C" {
#include "pico/bootrom.h" // For reset_usb_boot
}

#include "musin/midi/midi_wrapper.h" // For MIDI namespace and byte type

// --- Constants ---
// Define these locally if they are only needed for handle_sysex
#define SYSEX_DATO_ID 0x7D
#define SYSEX_DUO_ID 0x64
#define SYSEX_REBOOT_BOOTLOADER 0x0B

// --- Static Helper Functions (Internal Linkage) ---

// This function is only used as a callback pointer within midi_init,
// so it can remain static within this .cpp file.
static void handle_sysex(byte *const data, const unsigned length) {
  if (length > 3 && data[1] == SYSEX_DATO_ID && data[2] == SYSEX_DUO_ID &&
      data[3] == SYSEX_REBOOT_BOOTLOADER) {
    reset_usb_boot(0, 0);
  }
}

// --- Public Function Definitions (External Linkage) ---

void send_midi_cc(uint8_t channel, uint8_t cc_number, uint8_t value) {
  MIDI::sendControlChange(cc_number, value, channel);
}

void send_midi_note(uint8_t channel, uint8_t note_number, uint8_t velocity) {
  // The underlying library handles Note On/Off based on velocity
  // Use sendNoteOn for both Note On (velocity > 0) and Note Off (velocity == 0)
  MIDI::sendNoteOn(note_number, velocity, channel);
}

void midi_read() {
  // Process incoming MIDI messages
  MIDI::read();
}

void midi_init() {
  MIDI::init(MIDI::Callbacks{
      .note_on = nullptr, // Add callbacks here if needed later
      .note_off = nullptr,
      .clock = nullptr,
      .start = nullptr,
      .cont = nullptr,
      .stop = nullptr,
      .cc = nullptr,
      .pitch_bend = nullptr,
      .sysex = handle_sysex, // Use the static handler defined above
  });
}

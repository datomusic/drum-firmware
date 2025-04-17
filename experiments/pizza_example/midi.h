extern "C" {
  #include "pico/bootrom.h"
}

#include "musin/midi/midi_wrapper.h"

#define SYSEX_DATO_ID 0x7D
#define SYSEX_DUO_ID 0x64
#define SYSEX_REBOOT_BOOTLOADER 0x0B

static void handle_sysex(byte *const data, const unsigned length) {
  if (data[1] == SYSEX_DATO_ID && data[2] == SYSEX_DUO_ID && data[3] == SYSEX_REBOOT_BOOTLOADER) {
    reset_usb_boot(0, 0);
  }
}

// The actual MIDI sending function (prints and updates specific LEDs)
static void send_midi_cc([[maybe_unused]] uint8_t channel, uint8_t cc_number, uint8_t value) {
  // printf("MIDI CC %u: %u: %u\n", cc_number, value, channel);
  MIDI::sendControlChange(cc_number, value, channel);
}

static void send_midi_note([[maybe_unused]] uint8_t channel, uint8_t note_number, uint8_t velocity) {
  // printf("MIDI Note %u: %u\n", note_number, velocity);
  MIDI::sendNoteOn(note_number, velocity, channel);
}

static void midi_read() {
  MIDI::read();
}
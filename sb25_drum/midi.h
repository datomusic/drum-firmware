extern "C" {
#include "pico/bootrom.h"
}

#include "musin/midi/midi_wrapper.h"

#define SYSEX_DATO_ID 0x7D
#define SYSEX_DUO_ID 0x64
#define SYSEX_REBOOT_BOOTLOADER 0x0B

static void handle_sysex(byte *const data, const unsigned length) {
  if (length > 3 && data[1] == SYSEX_DATO_ID && data[2] == SYSEX_DUO_ID && data[3] == SYSEX_REBOOT_BOOTLOADER) {
      reset_usb_boot(0, 0);
  }
}

static void send_midi_cc(uint8_t channel, uint8_t cc_number, uint8_t value) {
  MIDI::sendControlChange(cc_number, value, channel);
}

static void send_midi_note(uint8_t channel, uint8_t note_number, uint8_t velocity) {
  MIDI::sendNoteOn(note_number, velocity, channel);
}

static void midi_read() {
  MIDI::read();
}

static void midi_init() {
  MIDI::init(MIDI::Callbacks{
      .note_on = nullptr,
      .note_off = nullptr,
      .clock = nullptr,
      .start = nullptr,
      .cont = nullptr,
      .stop = nullptr,
      .cc = nullptr,
      .pitch_bend = nullptr,
      .sysex = handle_sysex,
  });
}
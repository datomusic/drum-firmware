#include "midi.h"

extern "C" {
#include "pico/bootrom.h" // For reset_usb_boot
}

#include "musin/midi/midi_wrapper.h" // For MIDI namespace and byte type

// --- Constants ---
// Define these locally if they are only needed for handle_sysex
#define SYSEX_REBOOT_BOOTLOADER 0x0B
#define SYSEX_DATO_ID 0x7D
#define SYSEX_UNIVERSAL_NONREALTIME_ID 0x7e
#define SYSEX_UNIVERSAL_REALTIME_ID 0x7f
#define SYSEX_DRUM_ID 0x65
#define SYSEX_ALL_ID 0x7f

#define SYSEX_FIRMWARE_VERSION 0x01
#define SYSEX_SERIAL_NUMBER 0x02
#define SYSEX_REBOOT_BOOTLOADER 0x0B
#define SYSEX_RESET_TRANSPOSE 0x0C
#define SYSEX_SELFTEST 0x0A

// --- Static Helper Functions (Internal Linkage) ---

// This function is only used as a callback pointer within midi_init,
// so it can remain static within this .cpp file.
static void handle_sysex(uint8_t *const data, const size_t length) {
  if (length > 3 && data[1] == SYSEX_DATO_ID && data[2] == SYSEX_DRUM_ID) {
      switch(data[3]) {
        case SYSEX_REBOOT_BOOTLOADER:
          reset_usb_boot(0, 0);
          break;
        case SYSEX_FIRMWARE_VERSION:
          midi_print_firmware_version();
          break;
        case SYSEX_SERIAL_NUMBER:
          midi_print_serial_number();
          break;
  }

  if(data[1] == SYSEX_UNIVERSAL_NONREALTIME_ID) {
    if(data[2] == SYSEX_DRUM_ID || data[2] == SYSEX_ALL_ID) {
      if(data[3] == 06 && data[4] == 01) { // General Information Identity Request
        midi_print_identity();
      }
    }
  }
}

// --- Public Function Definitions (External Linkage) ---

void send_midi_cc(const uint8_t channel, const uint8_t cc_number, const uint8_t value) {
  MIDI::sendControlChange(cc_number, value, channel);
}

void send_midi_note(const uint8_t channel, const uint8_t note_number, const uint8_t velocity) {
  // The underlying library handles Note On/Off based on velocity
  // Use sendNoteOn for both Note On (velocity > 0) and Note Off (velocity == 0)
  MIDI::sendNoteOn(note_number, velocity, channel);
}

void send_midi_start() {
  MIDI::sendRealTime(midi::Start);
}

void send_midi_stop() {
  MIDI::sendRealTime(midi::Stop);
}

void midi_read() {
  MIDI::read();
}

void midi_init() {
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

void midi_print_identity() {

  uint8_t sysex[] = { 
    0xf0,
    0x7e,
    SYSEX_DRUM_ID,
    0x06, // General Information (sub-ID#1)
    0x02, // Identity Reply (sub-ID#2)
    SYSEX_DATO_ID, // Manufacturers System Exclusive id code
    0x00, 0x00, // Device family code (14 bits, LSB first)
    0x00, 0x00, // Device family member code (14 bits, LSB first)
    FIRMWARE_VERSION[0], // Software revision level. Major version
    FIRMWARE_VERSION[1], // Software revision level. Minor version
    FIRMWARE_VERSION[2], // Software revision level. Revision
    0xf7 };

  MIDI::sendSysEx(sizeof(sysex), sysex);
}

void midi_print_firmware_version() {

  uint8_t sysex[] = { 
    0xf0,
    SYSEX_DATO_ID,
    SYSEX_DUO_ID,
    FIRMWARE_VERSION[0],
    FIRMWARE_VERSION[1],
    FIRMWARE_VERSION[2],
    0xf7 };

  MIDI::sendSysEx(7, sysex);
}

void midi_print_serial_number() {
  // Serial number is sent as 4 groups of 5 7bit values, right aligned
  uint8_t sysex[24];

  sysex[0] = 0xf0;
  sysex[1] = SYSEX_DRUM_ID;
  sysex[2] = SYSEX_DUO_ID;

  sysex[3] = (SIM_UIDH >> 28) & 0x7f;
  sysex[4] = (SIM_UIDH >> 21) & 0x7f;
  sysex[5] = (SIM_UIDH >> 14) & 0x7f;
  sysex[6] = (SIM_UIDH >> 7) & 0x7f;
  sysex[7] = (SIM_UIDH) & 0x7f;

  sysex[8]  = (SIM_UIDMH >> 28) & 0x7f;
  sysex[9]  = (SIM_UIDMH >> 21) & 0x7f;
  sysex[10] = (SIM_UIDMH >> 14) & 0x7f;
  sysex[11] = (SIM_UIDMH >> 7) & 0x7f;
  sysex[12] = (SIM_UIDMH) & 0x7f;

  sysex[13] = (SIM_UIDML >> 28) & 0x7f;
  sysex[14] = (SIM_UIDML >> 21) & 0x7f;
  sysex[15] = (SIM_UIDML >> 14) & 0x7f;
  sysex[16] = (SIM_UIDML >> 7) & 0x7f;
  sysex[17] = (SIM_UIDML) & 0x7f;

  sysex[18] = (SIM_UIDL >> 28) & 0x7f;
  sysex[19] = (SIM_UIDL >> 21) & 0x7f;
  sysex[20] = (SIM_UIDL >> 14) & 0x7f;
  sysex[21] = (SIM_UIDL >> 7) & 0x7f;
  sysex[22] = (SIM_UIDL) & 0x7f;

  sysex[23]= 0xf7;

  MIDI::sendSysEx(24, sysex);
}

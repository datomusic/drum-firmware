#ifndef DUO_MIDI_FUNCTIONS_H
#define DUO_MIDI_FUNCTIONS_H

/*
  Port of duo-imxrt shared/duo/MidiFunctions.h (§7.6 minimal-change).

  Like the original this is a definition-carrying header, included once from
  the middle of duo/main.cpp's anonymous namespace, after `synth`, `transpose`,
  `note_off()` and `sequencer` exist. It is not a standalone translation unit.

  Dato DUO MIDI implementation chart

  Out only:
  MIDI CC 7   Volume
  MIDI CC 65  Glide 0 to 63 = Off, 64 to 127 = On
  MIDI CC 70  Pulse width
  MIDI CC 71  Filter Resonance
  MIDI CC 72  VCA Release Time
  MIDI CC 74  Filter cutoff
  MIDI CC 80  Delay 0 to 63 = Off, 64 to 127 = On
  MIDI CC 81  Crush 0 to 63 = Off, 64 to 127 = On
  MIDI CC 94  Detune amount

  Sysex:
  Send f0 7d 64 0b f7 to reboot into bootloader mode

  Send    f0 7d 64 01 f7 to retrieve firmware version
  Returns f0 7d 64 major_version minor_version patch f7

  Send    f0 7d 64 02 f7 to retrieve the serial number, sent as 4 groups of 5
          7-bit values, right aligned. The RP2350's unique id is 64 bits where
          the i.MX's was 128, so the leading two groups are always zero.

  Send    f0 7e 64 06 01 f7 to retrieve the identity
*/

#include "musin/midi/midi_wrapper.h"
#include "version.h"

#include <cstdint>

extern "C" {
#include "pico/bootrom.h"
#include "pico/unique_id.h"
}

constexpr uint8_t SYSEX_DATO_ID = 0x7D;
constexpr uint8_t SYSEX_UNIVERSAL_NONREALTIME_ID = 0x7E;
constexpr uint8_t SYSEX_DUO_ID = 0x64;
constexpr uint8_t SYSEX_ALL_ID = 0x7F;

constexpr uint8_t SYSEX_FIRMWARE_VERSION = 0x01;
constexpr uint8_t SYSEX_SERIAL_NUMBER = 0x02;
constexpr uint8_t SYSEX_SELFTEST = 0x0A;
constexpr uint8_t SYSEX_REBOOT_BOOTLOADER = 0x0B;
constexpr uint8_t SYSEX_RESET_TRANSPOSE = 0x0C;

constexpr uint8_t FIRMWARE_VERSION_BYTES[3] = {FIRMWARE_MAJOR, FIRMWARE_MINOR,
                                               FIRMWARE_PATCH};

// Last value sent per parameter, so only changes go out.
synth_parameters midi_parameters;

bool _midi_synth_value_changed(const int midi_param, const int synth_val) {
  return ((midi_param > (synth_val >> 3) + 1) ||
          (midi_param < (synth_val >> 3) - 1));
}

void _midi_send_changed_value(const int cc_num, int &midi_param,
                              const int synth_val) {
  if (_midi_synth_value_changed(midi_param, synth_val)) {
    MIDI::sendControlChange(cc_num, (synth_val >> 3), MIDI_CHANNEL);
    midi_param = ((synth_val >> 3) + midi_param) / 2;
  }
}

void _midi_send_changed_toggle(const int cc_num, bool &midi_param,
                               const bool synth_val) {
  if (midi_param != synth_val) {
    MIDI::sendControlChange(cc_num, (synth_val ? 127 : 0), MIDI_CHANNEL);
    midi_param = synth_val;
  }
}

#define send_changed_value(param_name, cc_num)                                 \
  _midi_send_changed_value(cc_num, midi_parameters.param_name, synth.param_name)

#define send_changed_toggle(param_name, cc_num)                                \
  _midi_send_changed_toggle(cc_num, midi_parameters.param_name,                \
                            synth.param_name)

void midi_send_cc() {
  send_changed_value(amplitude, 7);   // Volume
  send_changed_value(filter, 74);     // Filter 40 - 380
  send_changed_value(resonance, 71);  // Resonance 0.7 - 4.0
  send_changed_value(release, 72);    // Release time 30 - 500
  send_changed_value(pulseWidth, 70); // Pulse width
  send_changed_value(detune, 94);     // Detune
  send_changed_toggle(glide, 65);
  send_changed_toggle(delay, 80);
  send_changed_toggle(crush, 81);
}

#undef send_changed_value
#undef send_changed_toggle

void midi_note_on(uint8_t /*channel*/, uint8_t note, uint8_t velocity) {
  sequencer.hold_note(note, velocity);
}

void midi_note_off(uint8_t /*channel*/, uint8_t note, uint8_t /*velocity*/) {
  sequencer.release_note(note);
}

void midi_handle_cc(uint8_t channel, uint8_t number, uint8_t /*value*/) {
  if (channel == MIDI_CHANNEL) {
    switch (number) {
    case 123: // All notes off
      note_off();
      sequencer.release_all_notes();
      break;
    default:
      break;
    }
  }
}

void midi_print_identity() {
  const uint8_t sysex[] = {
      0xF0,
      SYSEX_UNIVERSAL_NONREALTIME_ID,
      SYSEX_DUO_ID,
      0x06,          // General Information (sub-ID#1)
      0x02,          // Identity Reply (sub-ID#2)
      SYSEX_DATO_ID, // Manufacturer System Exclusive id code
      0x00,
      0x00, // Device family code (14 bits, LSB first)
      0x00,
      0x00,                      // Family member code (14 bits, LSB first)
      FIRMWARE_VERSION_BYTES[0], // Software revision level. Major
      FIRMWARE_VERSION_BYTES[1], // Minor
      FIRMWARE_VERSION_BYTES[2], // Revision
      0xF7};

  MIDI::sendSysEx(sizeof(sysex), sysex);
}

void midi_print_firmware_version() {
  const uint8_t sysex[] = {0xF0,
                           SYSEX_DATO_ID,
                           SYSEX_DUO_ID,
                           FIRMWARE_VERSION_BYTES[0],
                           FIRMWARE_VERSION_BYTES[1],
                           FIRMWARE_VERSION_BYTES[2],
                           0xF7};

  MIDI::sendSysEx(sizeof(sysex), sysex);
}

void midi_print_serial_number() {
  // Serial number is sent as 4 groups of 5 7-bit values, right aligned. The
  // RP2350's board id is 64 bits, so it fills the low two groups only.
  pico_unique_board_id_t board_id;
  pico_get_unique_board_id(&board_id);

  uint32_t words[4] = {0, 0, 0, 0};
  for (int i = 0; i < 4; ++i) {
    words[2] = (words[2] << 8) | board_id.id[i];
    words[3] = (words[3] << 8) | board_id.id[i + 4];
  }

  uint8_t sysex[24];
  sysex[0] = 0xF0;
  sysex[1] = SYSEX_DATO_ID;
  sysex[2] = SYSEX_DUO_ID;
  for (int group = 0; group < 4; ++group) {
    for (int digit = 0; digit < 5; ++digit) {
      sysex[3 + group * 5 + digit] = (words[group] >> (28 - digit * 7)) & 0x7F;
    }
  }
  sysex[23] = 0xF7;

  MIDI::sendSysEx(sizeof(sysex), sysex);
}

// `data` is the complete message, 0xF0 first, as delivered by MIDI::read().
void midi_handle_sysex(uint8_t *data, unsigned length) {
  if (length < 5) {
    return;
  }

  if (data[1] == SYSEX_DATO_ID && data[2] == SYSEX_DUO_ID) {
    switch (data[3]) {
    case SYSEX_FIRMWARE_VERSION:
      midi_print_firmware_version();
      break;
    case SYSEX_SERIAL_NUMBER:
      midi_print_serial_number();
      break;
    case SYSEX_SELFTEST:
      // No selftest on this surface yet.
      break;
    case SYSEX_RESET_TRANSPOSE:
      transpose = 0;
      break;
    case SYSEX_REBOOT_BOOTLOADER:
      reset_usb_boot(0, 0);
      break;
    default:
      break;
    }
  }

  if (data[1] == SYSEX_UNIVERSAL_NONREALTIME_ID &&
      (data[2] == SYSEX_DUO_ID || data[2] == SYSEX_ALL_ID)) {
    // General Information Identity Request
    if (length >= 6 && data[3] == 0x06 && data[4] == 0x01) {
      midi_print_identity();
    }
  }
}

#endif /* DUO_MIDI_FUNCTIONS_H */

#include "midi_functions.h"

extern "C" {
#include "pico/bootrom.h"   // For reset_usb_boot
#include "pico/unique_id.h" // For pico_get_unique_board_id
}

#include "musin/midi/midi_wrapper.h" // For MIDI namespace and byte type
#include "version.h"                 // For FIRMWARE_MAJOR, FIRMWARE_MINOR, FIRMWARE_PATCH

// --- Constants ---
#define SYSEX_DATO_ID 0x7D // Manufacturer ID for Dato
#define SYSEX_UNIVERSAL_NONREALTIME_ID 0x7E
#define SYSEX_UNIVERSAL_REALTIME_ID 0x7F // Kept for completeness, though not used here
#define SYSEX_DRUM_ID 0x65               // Device ID for DRUM
#define SYSEX_ALL_ID 0x7F                // Target all devices

// Command bytes for Dato/DRUM specific SysEx
#define SYSEX_FIRMWARE_VERSION 0x01  // Custom command to request firmware version
#define SYSEX_SERIAL_NUMBER 0x02     // Custom command to request serial number
#define SYSEX_REBOOT_BOOTLOADER 0x0B // Custom command to reboot to bootloader

// --- Forward Declarations for Static Helper Functions ---
static void midi_print_identity();
static void midi_print_firmware_version();
static void midi_print_serial_number();

// --- Static Helper Functions (Internal Linkage) ---

#include <stdio.h>
static void handle_sysex(uint8_t *const data, const size_t length) {
  printf("HANDLRE SYX\n");
  // Check for Dato Manufacturer ID and DRUM Device ID
  if (length > 3 && data[1] == SYSEX_DATO_ID && data[2] == SYSEX_DRUM_ID) {
    switch (data[3]) { // Check the command byte
    case SYSEX_REBOOT_BOOTLOADER:
      reset_usb_boot(0, 0);
      break;
    case SYSEX_FIRMWARE_VERSION: // Handle request for custom firmware version message
      midi_print_firmware_version();
      break;
    case SYSEX_SERIAL_NUMBER: // Handle request for custom serial number message
      midi_print_serial_number();
      break;
    }
  }
  // Check for Universal Non-Realtime SysEx messages
  else if (length > 4 && data[1] == SYSEX_UNIVERSAL_NONREALTIME_ID) {
    // Check if the message is targeted to DRUM or all devices
    if (data[2] == SYSEX_DRUM_ID || data[2] == SYSEX_ALL_ID) {
      // Check for General Information - Identity Request (06 01)
      if (data[3] == 0x06 && data[4] == 0x01) {
        midi_print_identity(); // Send the standard Identity Reply
      }
    }
  }
}

// --- Public Function Definitions (External Linkage) ---

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
      .sysex = handle_sysex, // Register the SysEx handler
  });
}

// --- Static Helper Function Implementations ---

static void midi_print_identity() {
  uint8_t sysex[] = {
      0xF0,
      SYSEX_UNIVERSAL_NONREALTIME_ID, // 0x7E
      SYSEX_DRUM_ID,                  // Target Device ID
      0x06,                           // General Information (sub-ID#1)
      0x02,                           // Identity Reply (sub-ID#2)
      SYSEX_DATO_ID, // Manufacturer's System Exclusive ID code (using single byte ID)
      0x00,          // Device family code LSB (set to 0)
      0x00,          // Device family code MSB (set to 0)
      0x00,          // Device family member code LSB (set to 0)
      0x00,          // Device family member code MSB (set to 0)
      (uint8_t)(FIRMWARE_MAJOR & 0x7F), // Software revision level Byte 1 (Major)
      (uint8_t)(FIRMWARE_MINOR & 0x7F), // Software revision level Byte 2 (Minor)
      (uint8_t)(FIRMWARE_PATCH & 0x7F), // Software revision level Byte 3 (Patch)
      (uint8_t)(FIRMWARE_COMMITS &
                0x7F), // Software revision level Byte 4 (Commits since tag, capped at 127)
      0xF7};

  MIDI::sendSysEx(sizeof(sysex), sysex);
}

static void midi_print_firmware_version() {
  uint8_t sysex[] = {0xF0,
                     SYSEX_DATO_ID,
                     SYSEX_DRUM_ID,
                     SYSEX_FIRMWARE_VERSION, // Command byte indicating firmware version reply
                     (uint8_t)(FIRMWARE_MAJOR & 0x7F),
                     (uint8_t)(FIRMWARE_MINOR & 0x7F),
                     (uint8_t)(FIRMWARE_PATCH & 0x7F),
                     0xF7};

  MIDI::sendSysEx(sizeof(sysex), sysex);
}

static void midi_print_serial_number() {
  pico_unique_board_id_t id;
  pico_get_unique_board_id(&id); // Get the 64-bit (8-byte) unique ID

  // Encode the 8-byte ID into 9 SysEx data bytes (7-bit encoding).
  // Payload: [ID0&7F, ID1&7F, ..., ID7&7F, MSBs]
  // MSBs byte contains the MSB of each original ID byte.
  uint8_t sysex[14]; // 1(F0) + 1(Manuf) + 1(Dev) + 1(Cmd) + 9(Data) + 1(F7) = 14 bytes

  sysex[0] = 0xF0;
  sysex[1] = SYSEX_DATO_ID;
  sysex[2] = SYSEX_DRUM_ID;
  sysex[3] = SYSEX_SERIAL_NUMBER; // Command byte

  uint8_t msbs = 0;
  for (int i = 0; i < 8; ++i) {
    sysex[4 + i] = id.id[i] & 0x7F;        // Store the lower 7 bits
    msbs |= ((id.id[i] >> 7) & 0x01) << i; // Store the MSB in the msbs byte
  }
  sysex[12] = msbs; // Store the collected MSBs as the 9th data byte
  sysex[13] = 0xF7;

  MIDI::sendSysEx(sizeof(sysex), sysex);
}

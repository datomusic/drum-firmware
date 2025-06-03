#include "midi_functions.h"
#include "sound_router.h" // For drum::SoundRouter

extern "C" {
#include "pico/bootrom.h"   // For reset_usb_boot
#include "pico/unique_id.h" // For pico_get_unique_board_id
}

#include "musin/midi/midi_wrapper.h" // For MIDI namespace and byte type
#include "version.h"                 // For FIRMWARE_MAJOR, FIRMWARE_MINOR, FIRMWARE_PATCH
#include "config.h"                  // For drum::config::NUM_TRACKS
#include <optional>                  // For std::optional

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
static void midi_note_on_callback(uint8_t channel, uint8_t note, uint8_t velocity);
static void midi_note_off_callback(uint8_t channel, uint8_t note, uint8_t velocity);
static void midi_cc_callback(uint8_t channel, uint8_t controller, uint8_t value);

// --- Static Variables ---
// Pointer to the SoundRouter instance, to be set in midi_init
static drum::SoundRouter *g_sound_router_ptr = nullptr;

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

void midi_init(drum::SoundRouter &sound_router) {
  g_sound_router_ptr = &sound_router;
  MIDI::init(MIDI::Callbacks{
      .note_on = midi_note_on_callback,
      .note_off = midi_note_off_callback,
      .clock = nullptr,
      .start = nullptr,
      .cont = nullptr,
      .stop = nullptr,
      .cc = midi_cc_callback,
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

// --- MIDI Callback Implementations ---

static void midi_cc_callback(uint8_t channel, uint8_t controller, uint8_t value) {
  if (g_sound_router_ptr == nullptr) {
    return;
  }

  float normalized_value = static_cast<float>(value) / 127.0f;
  std::optional<drum::Parameter> param_id_opt = std::nullopt;
  std::optional<uint8_t> resolved_track_index = std::nullopt;

  // Try per-track parameters first. These are sent on channels 1-4 by SoundRouter.
  // MIDI channels 1-4 map to track_index 0-3.
  if (channel >= 1 && channel <= drum::config::NUM_TRACKS) {
    uint8_t track_idx = channel - 1;
    switch (controller) {
    // PITCH: CC16 on Ch1 (Tr0), CC17 on Ch2 (Tr1), ..., CC19 on Ch4 (Tr3)
    case 16: // Track 1 Pitch in DATO DRUM MIDI chart is CC 21
      if (track_idx == 0) {
        param_id_opt = drum::Parameter::PITCH;
        resolved_track_index = track_idx;
      }
      break;
    case 17: // Track 2 Pitch in DATO DRUM MIDI chart is CC 22
      if (track_idx == 1) {
        param_id_opt = drum::Parameter::PITCH;
        resolved_track_index = track_idx;
      }
      break;
    case 18: // Track 3 Pitch in DATO DRUM MIDI chart is CC 23
      if (track_idx == 2) {
        param_id_opt = drum::Parameter::PITCH;
        resolved_track_index = track_idx;
      }
      break;
    case 19: // Track 4 Pitch in DATO DRUM MIDI chart is CC 24
      if (track_idx == 3) {
        param_id_opt = drum::Parameter::PITCH;
        resolved_track_index = track_idx;
      }
      break;

    // DRUM_PRESSURE_X: CC20 for DP1 on Ch1 (Tr0), CC21 for DP2 on Ch2 (Tr1), ...
    // These are custom CCs not in the DATO DRUM chart, used internally.
    case 20:
      if (track_idx == 0) {
        param_id_opt = drum::Parameter::DRUM_PRESSURE_1;
        resolved_track_index = track_idx;
      }
      break;
    case 21:
      if (track_idx == 1) {
        param_id_opt = drum::Parameter::DRUM_PRESSURE_2;
        resolved_track_index = track_idx;
      }
      break;
    case 22:
      if (track_idx == 2) {
        param_id_opt = drum::Parameter::DRUM_PRESSURE_3;
        resolved_track_index = track_idx;
      }
      break;
    case 23:
      if (track_idx == 3) {
        param_id_opt = drum::Parameter::DRUM_PRESSURE_4;
        resolved_track_index = track_idx;
      }
      break;
    default:
      // Also check for DATO DRUM spec per-track pitch CCs if channel matches
      // DATO DRUM MIDI chart uses CCs 21-24 for Track 1-4 Pitch respectively, on default channel 10.
      // However, our SoundRouter sends pitch on per-track channels 1-4 using CCs 16-19.
      // For input, we should prioritize what SoundRouter sends.
      // If we want to *also* support the DATO DRUM spec for input (e.g. CC21 on Ch10 for Track 1 Pitch),
      // that would require additional logic, potentially checking channel 10 specifically.
      // For now, this callback primarily mirrors SoundRouter's output CC mapping for input.
      break;
    }
  }

  // If not matched as a per-track parameter, try global parameters.
  // Global parameters are sent on channel 1 by SoundRouter, so we expect them on channel 1.
  // The DATO DRUM MIDI chart specifies default channel 10 for most percussion controls.
  // We will listen on channel 1 as per SoundRouter's output behavior for these.
  if (!param_id_opt.has_value() && channel == 1) {
    switch (controller) {
    case 7: // Master Volume (DATO CC 7)
      param_id_opt = drum::Parameter::VOLUME;
      break;
    case 9: // Swing (DATO CC 9)
      param_id_opt = drum::Parameter::SWING;
      break;
    // case 12: // Crush Effect (DATO CC 12) - Our CRUSH_RATE/DEPTH are on 77/78
    //   break;
    case 15: // Tempo (DATO CC 15)
      param_id_opt = drum::Parameter::TEMPO_BPM;
      break;
    case 16: // Random Effect (DATO CC 16)
      param_id_opt = drum::Parameter::RANDOM_EFFECT;
      break;
    case 17: // Repeat Effect (DATO CC 17)
      param_id_opt = drum::Parameter::REPEAT_EFFECT;
      break;
    case 74: // Filter Cutoff (DATO CC 74) - Our FILTER_FREQUENCY is on 75
      // param_id_opt = drum::Parameter::FILTER_FREQUENCY; // If we want to map DATO's CC 74
      break;
    case 75: // Filter Resonance (DATO CC 75) - Our FILTER_RESONANCE is on 76
             // SoundRouter maps FILTER_FREQUENCY to CC 75
      param_id_opt = drum::Parameter::FILTER_FREQUENCY;
      break;
    case 76: // SoundRouter maps FILTER_RESONANCE to CC 76
      param_id_opt = drum::Parameter::FILTER_RESONANCE;
      break;
    case 77: // SoundRouter maps CRUSH_RATE to CC 77
      param_id_opt = drum::Parameter::CRUSH_RATE;
      break;
    case 78: // SoundRouter maps CRUSH_DEPTH to CC 78
      param_id_opt = drum::Parameter::CRUSH_DEPTH;
      break;
    default:
      break;
    }
    // For global parameters, resolved_track_index remains std::nullopt.
  }

  if (param_id_opt.has_value()) {
    g_sound_router_ptr->set_parameter(param_id_opt.value(), normalized_value, resolved_track_index);
  }
}

static void midi_note_on_callback(uint8_t channel, uint8_t note, uint8_t velocity) {
  // Process note events only on MIDI channel 10 (GM Percussion Standard)
  if (channel == 10) {
    if (g_sound_router_ptr) {
      g_sound_router_ptr->handle_incoming_midi_note(note, velocity);
    }
  }
}

static void midi_note_off_callback(uint8_t channel, uint8_t note, uint8_t velocity) {
  // Process note events only on MIDI channel 10 (GM Percussion Standard)
  if (channel == 10) {
    // MIDI Note Off can be represented as Note On with velocity 0,
    // or by a distinct Note Off message.
    // Pass 0 velocity to handle_incoming_midi_note to signify note off.
    if (g_sound_router_ptr) {
      g_sound_router_ptr->handle_incoming_midi_note(note, 0);
    }
  }
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

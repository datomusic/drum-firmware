#include "midi_functions.h"
#include "sound_router.h" // For drum::SoundRouter

extern "C" {
#include "pico/bootrom.h"   // For reset_usb_boot
#include "pico/unique_id.h" // For pico_get_unique_board_id
}

#include "config.h"                            // For drum::config::NUM_TRACKS
#include "musin/midi/midi_wrapper.h"           // For MIDI namespace and byte type
#include "musin/timing/midi_clock_processor.h" // For MidiClockProcessor
#include "sequencer_controller.h"              // For SequencerController
#include "version.h"                           // For FIRMWARE_MAJOR, FIRMWARE_MINOR, FIRMWARE_PATCH
#include <optional>                            // For std::optional

// --- Static Variables ---
// Pointers to global objects, to be set in midi_init
static drum::SoundRouter *sound_router_ptr = nullptr;
static drum::SequencerController<drum::config::NUM_TRACKS, drum::config::NUM_STEPS_PER_TRACK>
    *sequencer_controller_ptr = nullptr;
static musin::timing::MidiClockProcessor *midi_clock_processor_ptr = nullptr;

// --- Helper Functions (Internal Linkage) ---
namespace { // Anonymous namespace for internal linkage

// --- Constants ---
static constexpr uint8_t SYSEX_DATO_ID = 0x7D; // Manufacturer ID for Dato
static constexpr uint8_t SYSEX_UNIVERSAL_NONREALTIME_ID = 0x7E;
static constexpr uint8_t SYSEX_UNIVERSAL_REALTIME_ID =
    0x7F; // Kept for completeness, though not used here
static constexpr uint8_t SYSEX_DRUM_ID = 0x65; // Device ID for DRUM
static constexpr uint8_t SYSEX_ALL_ID = 0x7F;  // Target all devices

// Command bytes for Dato/DRUM specific SysEx
static constexpr uint8_t SYSEX_FIRMWARE_VERSION = 0x01;  // Custom command to request firmware version
static constexpr uint8_t SYSEX_SERIAL_NUMBER = 0x02;     // Custom command to request serial number
static constexpr uint8_t SYSEX_REBOOT_BOOTLOADER = 0x0B; // Custom command to reboot to bootloader

#include <stdio.h>
// Forward Declarations for Helper Functions within anonymous namespace
void midi_print_identity();
void midi_print_firmware_version();
void midi_print_serial_number();
void midi_note_on_callback(uint8_t channel, uint8_t note, uint8_t velocity);
void midi_note_off_callback(uint8_t channel, uint8_t note, [[maybe_unused]] uint8_t velocity);
void midi_cc_callback(uint8_t channel, uint8_t controller, uint8_t value);
void midi_start_input_callback();
void midi_stop_input_callback();
void midi_continue_input_callback();
void midi_clock_input_callback();
void handle_sysex(uint8_t *const data, const size_t length);

void handle_sysex(uint8_t *const data, const size_t length) {
  printf("HANDLE SYSEX\n");
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

// --- Helper Function Implementations ---

void midi_print_identity() {
  static constexpr uint8_t sysex[] = {
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

void midi_start_input_callback() {
  if (sequencer_controller_ptr) {
    // Per DATO chart: Begin/resume playback from current step
    sequencer_controller_ptr->start();
  }
}

void midi_stop_input_callback() {
  if (sequencer_controller_ptr) {
    // Per DATO chart: Stop playback, maintain step position
    sequencer_controller_ptr->stop();
  }
}

void midi_continue_input_callback() {
  if (sequencer_controller_ptr) {
    // Per DATO chart: Begin/resume playback from current step (same as Start)
    sequencer_controller_ptr->start();
  }
}

void midi_clock_input_callback() {
  if (midi_clock_processor_ptr) {
    midi_clock_processor_ptr->on_midi_clock_tick_received();
  }
}

// --- MIDI Callback Implementations ---

void midi_cc_callback(uint8_t channel, uint8_t controller, uint8_t value) {
  if (sound_router_ptr == nullptr) {
    return;
  }

  // Process CCs only if local control is OFF
  if (sound_router_ptr->get_local_control_mode() == drum::LocalControlMode::OFF) {
    float normalized_value = static_cast<float>(value) / 127.0f;

    // Process CCs on the configured default MIDI channel
    if (channel == drum::config::DEFAULT_MIDI_CHANNEL) {
      std::optional<drum::Parameter> param_id_opt = std::nullopt;
      std::optional<uint8_t> resolved_track_index = std::nullopt;

      // Map CC numbers to parameters based on DATO_Drum_midi_implementation_chart.md
      // Global Controls (on default channel)
      if (controller == 7) { // Master Volume
        param_id_opt = drum::Parameter::VOLUME;
      } else if (controller == 9) { // Swing
        param_id_opt = drum::Parameter::SWING;
      } else if (controller == 12) { // Crush Effect
        param_id_opt = drum::Parameter::CRUSH_EFFECT;
      } else if (controller == 15) { // Tempo
        param_id_opt = drum::Parameter::TEMPO;
      } else if (controller == 16) { // Random Effect
        param_id_opt = drum::Parameter::RANDOM_EFFECT;
      } else if (controller == 17) { // Repeat Effect
        param_id_opt = drum::Parameter::REPEAT_EFFECT;
      } else if (controller == 74) { // Filter Cutoff
        param_id_opt = drum::Parameter::FILTER_FREQUENCY;
      } else if (controller == 75) { // Filter Resonance
        param_id_opt = drum::Parameter::FILTER_RESONANCE;
      }
      // Per-Track Pitch Controls (CC 21-24 on default channel)
      else if (controller >= 21 && controller <= 24) {
        param_id_opt = drum::Parameter::PITCH;
        resolved_track_index = controller - 21; // CC 21 -> Tr 0, CC 22 -> Tr 1, etc.
      }

      if (param_id_opt.has_value()) {
        if (resolved_track_index.has_value() &&
            resolved_track_index.value() >= drum::config::NUM_TRACKS) {
          // Invalid track index derived from CC
          return;
        }
        sound_router_ptr->set_parameter(param_id_opt.value(), normalized_value,
                                        resolved_track_index);
      }
    }
  }
}

void midi_note_on_callback(uint8_t channel, uint8_t note, uint8_t velocity) {
  // Process note events only on the configured default MIDI channel
  if (channel == drum::config::DEFAULT_MIDI_CHANNEL) {
    if (sound_router_ptr) {
      sound_router_ptr->handle_incoming_midi_note(note, velocity);
    }
  }
}

void midi_note_off_callback(uint8_t channel, uint8_t note, [[maybe_unused]] uint8_t velocity) {
  // Process note events only on the configured default MIDI channel
  if (channel == drum::config::DEFAULT_MIDI_CHANNEL) {
    // MIDI Note Off can be represented as Note On with velocity 0,
    // or by a distinct Note Off message.
    // Pass 0 velocity to handle_incoming_midi_note to signify note off.
    if (sound_router_ptr) {
      sound_router_ptr->handle_incoming_midi_note(note, 0);
    }
  }
}

void midi_print_firmware_version() {
  static constexpr uint8_t sysex[] = {
      0xF0,
      SYSEX_DATO_ID,
      SYSEX_DRUM_ID,
      SYSEX_FIRMWARE_VERSION, // Command byte indicating firmware version reply
                     (uint8_t)(FIRMWARE_MAJOR & 0x7F),
                     (uint8_t)(FIRMWARE_MINOR & 0x7F),
                     (uint8_t)(FIRMWARE_PATCH & 0x7F),
                     0xF7};

  MIDI::sendSysEx(sizeof(sysex), sysex);
}

void midi_print_serial_number() {
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

} // anonymous namespace

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

void midi_init(drum::SoundRouter &sound_router,
               drum::SequencerController<drum::config::NUM_TRACKS,
                                         drum::config::NUM_STEPS_PER_TRACK> &sequencer_controller,
               musin::timing::MidiClockProcessor &midi_clock_processor) {
  sound_router_ptr = &sound_router;
  sequencer_controller_ptr = &sequencer_controller;
  midi_clock_processor_ptr = &midi_clock_processor; // Store reference to MIDI clock processor
  MIDI::init(MIDI::Callbacks{
      .note_on = midi_note_on_callback,
      .note_off = midi_note_off_callback,
      .clock = midi_clock_input_callback, // Register MIDI clock callback
      .start = midi_start_input_callback,
      .cont = midi_continue_input_callback,
      .stop = midi_stop_input_callback,
      .cc = midi_cc_callback,
      .pitch_bend = nullptr,
      .sysex = handle_sysex, // Register the SysEx handler
  });
}

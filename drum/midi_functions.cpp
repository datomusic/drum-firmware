#include "midi_functions.h"
#include "sound_router.h" // For drum::SoundRouter

extern "C" {
#include "pico/bootrom.h"   // For reset_usb_boot
#include "pico/unique_id.h" // For pico_get_unique_board_id
}

#include "config.h" // For drum::config::NUM_TRACKS
#include "musin/hal/logger.h"
#include "musin/midi/midi_wrapper.h"           // For MIDI namespace and byte type
#include "musin/timing/midi_clock_processor.h" // For MidiClockProcessor
#include "sequencer_controller.h"              // For SequencerController
#include "sysex/protocol.h"                    // For SysEx protocol handler
#include "version.h"                           // For FIRMWARE_MAJOR, FIRMWARE_MINOR, FIRMWARE_PATCH
#include <cassert>                             // For assert
#include <optional>                            // For std::optional

// --- Static Variables ---
// Pointers to global objects, to be set in midi_init
static drum::SoundRouter *sound_router_ptr = nullptr;
static drum::SequencerController<drum::config::NUM_TRACKS, drum::config::NUM_STEPS_PER_TRACK>
    *sequencer_controller_ptr = nullptr;
static musin::timing::MidiClockProcessor *midi_clock_processor_ptr = nullptr;
static sysex::Protocol<StandardFileOps> *sysex_protocol_ptr = nullptr;
static void (*file_received_callback_ptr)() = nullptr;
static musin::Logger *logger_ptr = nullptr;

// --- Helper Functions (Internal Linkage) ---
namespace { // Anonymous namespace for internal linkage

// --- Constants ---
static constexpr uint8_t SYSEX_UNIVERSAL_NONREALTIME_ID = 0x7E;
static constexpr uint8_t SYSEX_UNIVERSAL_REALTIME_ID = 0x7F; // Unused
static constexpr uint8_t SYSEX_ALL_ID = 0x7F;                // Target all devices

// Command bytes for Dato/DRUM specific SysEx are now defined in sysex::Protocol::Tag

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
  // Check for Universal Non-Realtime SysEx messages first.
  if (length > 4 && data[1] == SYSEX_UNIVERSAL_NONREALTIME_ID) {
    // Check if the message is targeted to DRUM or all devices.
    if (data[2] == drum::config::sysex::DEVICE_ID || data[2] == SYSEX_ALL_ID) {
      // Check for General Information - Identity Request (06 01).
      if (data[3] == 0x06 && data[4] == 0x01) {
        midi_print_identity(); // Send the standard Identity Reply.
        return;                // Handled.
      }
    }
    // Other universal messages are ignored.
    return;
  }

  // All other messages are assumed to be for our custom protocol.
  // printf("SysEx Custom: received %u bytes: ", (unsigned)length);
  // for (size_t i = 0; i < length; ++i) {
  //   printf("%02X ", data[i]);
  // }
  // printf("\n");

  assert(sysex_protocol_ptr != nullptr && "sysex_protocol_ptr must be initialized");
  assert(file_received_callback_ptr != nullptr && "file_received_callback_ptr must be initialized");

  // The sysex protocol handler expects the payload without the 0xF0/0xF7 framing bytes.
  sysex::Chunk chunk(data + 1, length - 2);

  // Define a sender lambda for ACK/NACK replies.
  auto sender = [](sysex::Protocol<StandardFileOps>::Tag tag) {
    uint8_t msg[] = {0xF0,
                     drum::config::sysex::MANUFACTURER_ID_0,
                     drum::config::sysex::MANUFACTURER_ID_1,
                     drum::config::sysex::MANUFACTURER_ID_2,
                     drum::config::sysex::DEVICE_ID,
                     static_cast<uint8_t>(tag), // The reply tag (Ack/Nack)
                     0xF7};
    MIDI::sendSysEx(sizeof(msg), msg);
  };

  auto result = sysex_protocol_ptr->handle_chunk(chunk, sender);

  // Handle the result from the protocol handler.
  switch (result) {
  case sysex::Protocol<StandardFileOps>::Result::FileWritten:
    file_received_callback_ptr();
    break;
  case sysex::Protocol<StandardFileOps>::Result::Reboot:
    reset_usb_boot(0, 0);
    break;
  case sysex::Protocol<StandardFileOps>::Result::PrintFirmwareVersion:
    midi_print_firmware_version();
    break;
  case sysex::Protocol<StandardFileOps>::Result::PrintSerialNumber:
    midi_print_serial_number();
    break;
  // Other results like OK, FileError, etc. are handled within the protocol
  // and don't require action here.
  default:
    break;
  }
}

// --- Helper Function Implementations ---

void midi_print_identity() {
  static constexpr uint8_t sysex[] = {
      0xF0,
      SYSEX_UNIVERSAL_NONREALTIME_ID,         // 0x7E
      drum::config::sysex::DEVICE_ID,         // Target Device ID
      0x06,                                   // General Information (sub-ID#1)
      0x02,                                   // Identity Reply (sub-ID#2)
      drum::config::sysex::MANUFACTURER_ID_0, // Manufacturer's System Exclusive ID
      drum::config::sysex::MANUFACTURER_ID_1,
      drum::config::sysex::MANUFACTURER_ID_2,
      0x00,                             // Device family code LSB (set to 0)
      0x00,                             // Device family code MSB (set to 0)
      0x00,                             // Device family member code LSB (set to 0)
      0x00,                             // Device family member code MSB (set to 0)
      (uint8_t)(FIRMWARE_MAJOR & 0x7F), // Software revision level Byte 1 (Major)
      (uint8_t)(FIRMWARE_MINOR & 0x7F), // Software revision level Byte 2 (Minor)
      (uint8_t)(FIRMWARE_PATCH & 0x7F), // Software revision level Byte 3 (Patch)
      (uint8_t)(FIRMWARE_COMMITS &
                0x7F), // Software revision level Byte 4 (Commits since tag, capped at 127)
      0xF7};

  MIDI::sendSysEx(sizeof(sysex), sysex);
}

void midi_start_input_callback() {
  assert(sequencer_controller_ptr != nullptr &&
         "sequencer_controller_ptr must be initialized via midi_init()");
  // Per DATO chart: Begin/resume playback from current step
  sequencer_controller_ptr->start();
}

void midi_stop_input_callback() {
  assert(sequencer_controller_ptr != nullptr &&
         "sequencer_controller_ptr must be initialized via midi_init()");
  // Per DATO chart: Stop playback, maintain step position
  sequencer_controller_ptr->stop();
}

void midi_continue_input_callback() {
  assert(sequencer_controller_ptr != nullptr &&
         "sequencer_controller_ptr must be initialized via midi_init()");
  // Per DATO chart: Begin/resume playback from current step (same as Start)
  sequencer_controller_ptr->start();
}

void midi_clock_input_callback() {
  assert(midi_clock_processor_ptr != nullptr &&
         "midi_clock_processor_ptr must be initialized via midi_init()");
  midi_clock_processor_ptr->on_midi_clock_tick_received();
}

// --- MIDI Callback Implementations ---

void midi_cc_callback(uint8_t channel, uint8_t controller, uint8_t value) {
  assert(sound_router_ptr != nullptr && "sound_router_ptr must be initialized via midi_init()");

  // Process CCs only if local control is OFF
  if (sound_router_ptr->get_local_control_mode() == drum::LocalControlMode::OFF) {
    float normalized_value = static_cast<float>(value) / 127.0f;

    // Process CCs on the configured default MIDI channel
    if (channel == drum::config::FALLBACK_MIDI_CHANNEL) {
      std::optional<drum::Parameter> param_id_opt = std::nullopt;
      std::optional<uint8_t> resolved_track_index = std::nullopt;

      // Map CC numbers to parameters based on DATO_Drum_midi_implementation_chart.md
      switch (controller) {
      // Global Controls (on default channel)
      case 7: // Master Volume
        param_id_opt = drum::Parameter::VOLUME;
        break;
      case 9: // Swing
        param_id_opt = drum::Parameter::SWING;
        break;
      case 12: // Crush Effect
        param_id_opt = drum::Parameter::CRUSH_EFFECT;
        break;
      case 15: // Tempo
        param_id_opt = drum::Parameter::TEMPO;
        break;
      case 16: // Random Effect
        param_id_opt = drum::Parameter::RANDOM_EFFECT;
        break;
      case 17: // Repeat Effect
        param_id_opt = drum::Parameter::REPEAT_EFFECT;
        break;
      case 74: // Filter Cutoff
        param_id_opt = drum::Parameter::FILTER_FREQUENCY;
        break;
      case 75: // Filter Resonance
        param_id_opt = drum::Parameter::FILTER_RESONANCE;
        break;
      // Per-Track Pitch Controls (CC 21-24 on default channel)
      case 21: // Track 1 Pitch
        param_id_opt = drum::Parameter::PITCH;
        resolved_track_index = 0;
        break;
      case 22: // Track 2 Pitch
        param_id_opt = drum::Parameter::PITCH;
        resolved_track_index = 1;
        break;
      case 23: // Track 3 Pitch
        param_id_opt = drum::Parameter::PITCH;
        resolved_track_index = 2;
        break;
      case 24: // Track 4 Pitch
        param_id_opt = drum::Parameter::PITCH;
        resolved_track_index = 3;
        break;
      default:
        // Unhandled CC
        break;
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
  if (channel == drum::config::FALLBACK_MIDI_CHANNEL) {
    assert(sound_router_ptr != nullptr && "sound_router_ptr must be initialized via midi_init()");
    sound_router_ptr->handle_incoming_midi_note(note, velocity);
  }
}

void midi_note_off_callback(uint8_t channel, uint8_t note, [[maybe_unused]] uint8_t velocity) {
  // Process note events only on the configured default MIDI channel
  if (channel == drum::config::FALLBACK_MIDI_CHANNEL) {
    // MIDI Note Off can be represented as Note On with velocity 0,
    // or by a distinct Note Off message.
    // Pass 0 velocity to handle_incoming_midi_note to signify note off.
    assert(sound_router_ptr != nullptr && "sound_router_ptr must be initialized via midi_init()");
    sound_router_ptr->handle_incoming_midi_note(note, 0);
  }
}

void midi_print_firmware_version() {
  assert(logger_ptr != nullptr && "logger_ptr must be initialized");
  logger_ptr->info("Sending firmware version via SysEx");
  static constexpr uint8_t sysex[] = {
      0xF0,
      drum::config::sysex::MANUFACTURER_ID_0,
      drum::config::sysex::MANUFACTURER_ID_1,
      drum::config::sysex::MANUFACTURER_ID_2,
      drum::config::sysex::DEVICE_ID,
      static_cast<uint8_t>(
          sysex::Protocol<StandardFileOps>::Tag::RequestFirmwareVersion), // Command byte
      (uint8_t)(FIRMWARE_MAJOR & 0x7F),
      (uint8_t)(FIRMWARE_MINOR & 0x7F),
      (uint8_t)(FIRMWARE_PATCH & 0x7F),
      0xF7};

  MIDI::sendSysEx(sizeof(sysex), sysex);
}

void midi_print_serial_number() {
  pico_unique_board_id_t id;
  pico_get_unique_board_id(&id); // Get the 64-bit (8-byte) unique ID

  uint8_t sysex[16]; // 1(F0) + 3(Manuf) + 1(Dev) + 1(Cmd) + 9(Data) + 1(F7) = 16 bytes

  sysex[0] = 0xF0;
  sysex[1] = drum::config::sysex::MANUFACTURER_ID_0;
  sysex[2] = drum::config::sysex::MANUFACTURER_ID_1;
  sysex[3] = drum::config::sysex::MANUFACTURER_ID_2;
  sysex[4] = drum::config::sysex::DEVICE_ID;
  sysex[5] = static_cast<uint8_t>(
      sysex::Protocol<StandardFileOps>::Tag::RequestSerialNumber); // Command byte

  uint8_t msbs = 0;
  for (int i = 0; i < 8; ++i) {
    sysex[6 + i] = id.id[i] & 0x7F;        // Store the lower 7 bits
    msbs |= ((id.id[i] >> 7) & 0x01) << i; // Store the MSB in the msbs byte
  }
  sysex[14] = msbs; // Store the collected MSBs as the 9th data byte
  sysex[15] = 0xF7;

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
               musin::timing::MidiClockProcessor &midi_clock_processor,
               sysex::Protocol<StandardFileOps> &sysex_protocol, void (*file_received_cb)(),
               musin::Logger &logger) {
  sound_router_ptr = &sound_router;
  sequencer_controller_ptr = &sequencer_controller;
  midi_clock_processor_ptr = &midi_clock_processor;
  sysex_protocol_ptr = &sysex_protocol;
  file_received_callback_ptr = file_received_cb;
  logger_ptr = &logger;

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

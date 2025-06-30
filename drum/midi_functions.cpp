#include "midi_functions.h"
#include "message_router.h" // For drum::MessageRouter
#include "musin/midi/midi_input_queue.h"
#include "sysex_file_handler.h"

extern "C" {
#include "pico/bootrom.h"   // For reset_usb_boot
#include "pico/unique_id.h" // For pico_get_unique_board_id
}

#include "config.h" // For drum::config::NUM_TRACKS
#include "etl/delegate.h"
#include "musin/hal/logger.h"
#include "musin/midi/midi_wrapper.h"           // For MIDI namespace and byte type
#include "musin/timing/midi_clock_processor.h" // For MidiClockProcessor
#include "sequencer_controller.h"              // For SequencerController
#include "sysex/protocol.h"                    // For SysEx protocol handler
#include "version.h"                           // For FIRMWARE_MAJOR, FIRMWARE_MINOR, FIRMWARE_PATCH
#include <cassert>                             // For assert
#include <optional>                            // For std::optional

struct MidiHandlers {
  etl::delegate<void(uint8_t, uint8_t, uint8_t)> note_on;
  etl::delegate<void(uint8_t, uint8_t, uint8_t)> note_off;
  etl::delegate<void(uint8_t, uint8_t, uint8_t)> control_change;
  etl::delegate<void(const sysex::Chunk &)> sysex;
  etl::delegate<void(::midi::MidiType)> realtime;
  etl::delegate<void()> file_received;
};

MidiHandlers midi_handlers;

// --- Static Variables ---
static musin::Logger *logger_ptr = nullptr;
static sysex::Protocol<StandardFileOps> *sysex_protocol_ptr = nullptr;

// --- Helper Functions (Internal Linkage) ---
namespace { // Anonymous namespace for internal linkage

void midi_print_identity();
void midi_print_firmware_version();
void midi_print_serial_number();
void midi_note_on_callback(uint8_t channel, uint8_t note, uint8_t velocity);
void midi_note_off_callback(uint8_t channel, uint8_t note, [[maybe_unused]] uint8_t velocity);
void midi_cc_callback(uint8_t channel, uint8_t controller, uint8_t value);
void handle_sysex_callback(uint8_t *const data, unsigned length);
void midi_clock_callback();
void midi_start_callback();
void midi_continue_callback();
void midi_stop_callback();

void handle_sysex_callback(uint8_t *const data, unsigned length) {
  // The underlying MIDI library passes the full SysEx message, including the
  // start (0xF0) and end (0xF7) bytes. We need to slice these off for the
  // protocol handler.
  if (length < 2) {
    return; // Not a valid SysEx message.
  }
  sysex::Chunk chunk(data + 1, length - 2);
  musin::midi::enqueue_incoming_midi_message(musin::midi::IncomingMidiMessage(chunk));
}

void handle_note_on([[maybe_unused]] uint8_t channel, [[maybe_unused]] uint8_t note,
                    [[maybe_unused]] uint8_t velocity) {
  // TODO: Route MIDI note on events to the sound engine.
}

void handle_note_off([[maybe_unused]] uint8_t channel, [[maybe_unused]] uint8_t note,
                     [[maybe_unused]] uint8_t velocity) {
  // TODO: Route MIDI note off events to the sound engine.
}

void handle_control_change([[maybe_unused]] uint8_t channel, [[maybe_unused]] uint8_t controller,
                           [[maybe_unused]] uint8_t value) {
  // TODO: Route MIDI CC events to the sound engine/parameter manager.
}

void handle_sysex(const sysex::Chunk &chunk) {
  auto sender = [](sysex::Protocol<StandardFileOps>::Tag tag) {
    uint8_t msg[] = {0xF0,
                     drum::config::sysex::MANUFACTURER_ID_0,
                     drum::config::sysex::MANUFACTURER_ID_1,
                     drum::config::sysex::MANUFACTURER_ID_2,
                     drum::config::sysex::DEVICE_ID,
                     static_cast<uint8_t>(tag),
                     0xF7};
    MIDI::sendSysEx(sizeof(msg), msg);
  };

  assert(sysex_protocol_ptr != nullptr && "sysex_protocol_ptr must be initialized");
  auto result = sysex_protocol_ptr->template handle_chunk<decltype(sender)>(chunk, sender);

  switch (result) {
  case sysex::Protocol<StandardFileOps>::Result::FileWritten:
    midi_handlers.file_received();
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
  default:
    break;
  }
}

void midi_note_on_callback(uint8_t channel, uint8_t note, uint8_t velocity) {
  musin::midi::enqueue_incoming_midi_message(
      musin::midi::IncomingMidiMessage(channel, note, velocity, true));
}

void midi_note_off_callback(uint8_t channel, uint8_t note, uint8_t velocity) {
  musin::midi::enqueue_incoming_midi_message(
      musin::midi::IncomingMidiMessage(channel, note, velocity, false));
}

void midi_cc_callback(uint8_t channel, uint8_t controller, uint8_t value) {
  musin::midi::enqueue_incoming_midi_message(
      musin::midi::IncomingMidiMessage(channel, controller, value));
}

void midi_clock_callback() {
  musin::midi::enqueue_incoming_midi_message(musin::midi::IncomingMidiMessage(::midi::Clock));
}

void midi_start_callback() {
  musin::midi::enqueue_incoming_midi_message(musin::midi::IncomingMidiMessage(::midi::Start));
}

void midi_continue_callback() {
  musin::midi::enqueue_incoming_midi_message(musin::midi::IncomingMidiMessage(::midi::Continue));
}

void midi_stop_callback() {
  musin::midi::enqueue_incoming_midi_message(musin::midi::IncomingMidiMessage(::midi::Stop));
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
  pico_get_unique_board_id(&id);

  uint8_t sysex[16];
  sysex[0] = 0xF0;
  sysex[1] = drum::config::sysex::MANUFACTURER_ID_0;
  sysex[2] = drum::config::sysex::MANUFACTURER_ID_1;
  sysex[3] = drum::config::sysex::MANUFACTURER_ID_2;
  sysex[4] = drum::config::sysex::DEVICE_ID;
  sysex[5] = static_cast<uint8_t>(sysex::Protocol<StandardFileOps>::Tag::RequestSerialNumber);

  uint8_t msbs = 0;
  for (int i = 0; i < 8; ++i) {
    sysex[6 + i] = id.id[i] & 0x7F;
    msbs |= ((id.id[i] >> 7) & 0x01) << i;
  }
  sysex[14] = msbs;
  sysex[15] = 0xF7;

  MIDI::sendSysEx(sizeof(sysex), sysex);
}

} // anonymous namespace

void midi_process_input() {
  MIDI::read();

  musin::midi::IncomingMidiMessage message;
  while (musin::midi::dequeue_incoming_midi_message(message)) {
    switch (message.type) {
    case musin::midi::IncomingMidiMessageType::NOTE_ON:
      handle_note_on(message.data.note_message.channel, message.data.note_message.note,
                     message.data.note_message.velocity);
      break;
    case musin::midi::IncomingMidiMessageType::NOTE_OFF:
      handle_note_off(message.data.note_message.channel, message.data.note_message.note,
                      message.data.note_message.velocity);
      break;
    case musin::midi::IncomingMidiMessageType::CONTROL_CHANGE:
      handle_control_change(message.data.control_change_message.channel,
                            message.data.control_change_message.controller,
                            message.data.control_change_message.value);
      break;
    case musin::midi::IncomingMidiMessageType::SYSTEM_REALTIME:
      midi_handlers.realtime(message.data.system_realtime_message.type);
      break;
    case musin::midi::IncomingMidiMessageType::SYSTEM_EXCLUSIVE:
      handle_sysex(message.data.system_exclusive_message);
      break;
    }
  }
}

void midi_init(musin::timing::MidiClockProcessor &midi_clock_processor,
               drum::SysExFileHandler &sysex_file_handler, musin::Logger &logger) {
  logger_ptr = &logger;
  sysex_protocol_ptr = &sysex_file_handler.get_protocol();

  midi_handlers.note_on.set<handle_note_on>();
  midi_handlers.note_off.set<handle_note_off>();
  midi_handlers.control_change.set<handle_control_change>();
  midi_handlers.sysex.set<handle_sysex>();
  midi_handlers.realtime.set([&midi_clock_processor]([[maybe_unused]] ::midi::MidiType type) {
    midi_clock_processor.on_midi_clock_tick_received();
  });
  midi_handlers.file_received.set<drum::SysExFileHandler, &drum::SysExFileHandler::on_file_received>(
      sysex_file_handler);

  MIDI::init(MIDI::Callbacks{
      .note_on = midi_note_on_callback,
      .note_off = midi_note_off_callback,
      .clock = midi_clock_callback,
      .start = midi_start_callback,
      .cont = midi_continue_callback,
      .stop = midi_stop_callback,
      .cc = midi_cc_callback,
      .pitch_bend = nullptr,
      .sysex = handle_sysex_callback,
  });
}

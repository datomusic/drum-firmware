#ifndef DRUM_MIDI_MANAGER_H
#define DRUM_MIDI_MANAGER_H

#include <cstdint>

// Forward declarations to minimize header includes
namespace musin {
class Logger;
namespace timing {
class MidiClockProcessor;
} // namespace timing
} // namespace musin

namespace drum {
class MessageRouter;
class SysExFileHandler;

/**
 * @class MidiManager
 * @brief Encapsulates all MIDI I/O operations and routing.
 *
 * This class is responsible for initializing the underlying MIDI library,
 * handling its callbacks, and dispatching MIDI messages to the appropriate
 * handlers within the system (e.g., MessageRouter, SysExFileHandler).
 * It is designed to be the single point of contact for MIDI processing.
 */
class MidiManager {
public:
  /**
   * @brief Constructs the MidiManager.
   *
   * @param message_router Reference to the router for Note/CC messages.
   * @param midi_clock_processor Reference to the processor for clock messages.
   * @param sysex_file_handler Reference to the handler for SysEx messages.
   * @param logger Reference to the system logger.
   */
  MidiManager(MessageRouter &message_router,
              musin::timing::MidiClockProcessor &midi_clock_processor,
              SysExFileHandler &sysex_file_handler, musin::Logger &logger);

  /**
   * @brief Initializes the MIDI hardware and sets up callbacks.
   *
   * This must be called once before any MIDI processing can occur.
   */
  void init();

  /**
   * @brief Processes all pending incoming MIDI messages.
   *
   * This should be called repeatedly in the main application loop.
   */
  void process_input();

private:
  // --- Dependencies ---
  MessageRouter &message_router_;
  musin::timing::MidiClockProcessor &midi_clock_processor_;
  SysExFileHandler &sysex_file_handler_;
  musin::Logger &logger_;

  // --- Singleton Instance for C Callbacks ---
  // A raw pointer is used to interface with the C-style MIDI library,
  // which does not support context pointers in its callbacks.
  static MidiManager *instance_;

  // --- C-style Callbacks ---
  // These static functions are registered with the MIDI library. They capture
  // incoming data and queue it for processing in the main loop.
  static void note_on_callback(uint8_t channel, uint8_t note,
                               uint8_t velocity);
  static void note_off_callback(uint8_t channel, uint8_t note,
                                uint8_t velocity);
  static void cc_callback(uint8_t channel, uint8_t controller, uint8_t value);
  static void sysex_callback(uint8_t *data, unsigned length);
  static void clock_callback();
  static void start_callback();
  static void continue_callback();
  static void stop_callback();

  // --- Message Handlers ---
  // These methods are called by process_input() to act on dequeued messages.
  void handle_note_on(uint8_t channel, uint8_t note, uint8_t velocity);
  void handle_note_off(uint8_t channel, uint8_t note, uint8_t velocity);
  void handle_control_change(uint8_t channel, uint8_t controller,
                             uint8_t value);
  void handle_sysex(const etl::span<const uint8_t> &data);
  void handle_realtime(uint16_t type);
};

} // namespace drum

#endif // DRUM_MIDI_MANAGER_H

#ifndef SB25_DRUM_MIDI_FUNCTIONS_H
#define SB25_DRUM_MIDI_FUNCTIONS_H

#include "config.h"
#include <cstddef> // For size_t
#include <cstdint> // For uint8_t

// Forward declarations
namespace drum {
class MessageRouter;
template <size_t NumTracks, size_t NumSteps> class SequencerController;
class SysExFileHandler;
} // namespace drum

namespace musin {
class Logger;
namespace timing {
class MidiClockProcessor;
} // namespace timing
} // namespace musin

#include "drum/applications/rompler/standard_file_ops.h"

namespace sysex {
template <typename T> struct Protocol;
}

// Function declarations (prototypes) for functions defined in midi.cpp
/**
 * @brief Initialize the MIDI system and callbacks.
 * @param midi_clock_processor Reference to the MidiClockProcessor for handling MIDI clock input.
 * @param sysex_file_handler Reference to the SysExFileHandler.
 * @param logger Reference to the system logger.
 */
void midi_init(musin::timing::MidiClockProcessor &midi_clock_processor,
               drum::SysExFileHandler &sysex_file_handler, musin::Logger &logger);

/**
 * @brief Read and process incoming MIDI messages. Should be called periodically.
 */
void midi_process_input();

/**
 * @brief Send a MIDI Start message.
 */
void send_midi_start();

/**
 * @brief Send a MIDI Stop message.
 */
void send_midi_stop();

#endif // SB25_DRUM_MIDI_FUNCTIONS_H

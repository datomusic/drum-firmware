#ifndef SB25_DRUM_MIDI_FUNCTIONS_H
#define SB25_DRUM_MIDI_FUNCTIONS_H

#include "config.h"
#include <cstddef> // For size_t
#include <cstdint> // For uint8_t

// Forward declarations
namespace drum {
class SoundRouter;
template <size_t NumTracks, size_t NumSteps> class SequencerController; // Forward declare
// drum::config constants will be picked up from config.h
} // namespace drum

namespace musin {
namespace timing {
class MidiClockProcessor; // Forward declaration
} // namespace timing
} // namespace musin

// Function declarations (prototypes) for functions defined in midi.cpp
/**
 * @brief Initialize the MIDI system and callbacks.
 * @param sound_router Reference to the SoundRouter for handling MIDI note events.
 * @param sequencer_controller Reference to the SequencerController for transport control.
 * @param midi_clock_processor Reference to the MidiClockProcessor for handling MIDI clock input.
 */
void midi_init(drum::SoundRouter &sound_router,
               drum::SequencerController<drum::config::NUM_TRACKS,
                                         drum::config::NUM_STEPS_PER_TRACK> &sequencer_controller,
               musin::timing::MidiClockProcessor &midi_clock_processor);

/**
 * @brief Process incoming MIDI messages. Should be called periodically.
 */
void midi_read();

/**
 * @brief Send a MIDI Start message.
 */
void send_midi_start();

/**
 * @brief Send a MIDI Stop message.
 */
void send_midi_stop();

#endif // SB25_DRUM_MIDI_FUNCTIONS_H

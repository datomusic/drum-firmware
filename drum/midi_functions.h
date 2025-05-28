#ifndef SB25_DRUM_MIDI_FUNCTIONS_H
#define SB25_DRUM_MIDI_FUNCTIONS_H

#include <cstdint> // For uint8_t

// Forward declaration of the SequencerController template class
namespace drum {
template <size_t NumTracks, size_t NumSteps>
class SequencerController;
}

// Function declarations (prototypes) for functions defined in midi.cpp

/**
 * @brief Initialize the MIDI system and callbacks.
 */
void midi_init();

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

/**
 * @brief Set the reference to the sequencer controller for MIDI note handling.
 * @param controller Reference to the sequencer controller instance.
 */
template <size_t NumTracks, size_t NumSteps>
void set_sequencer_controller(drum::SequencerController<NumTracks, NumSteps>& controller);

#endif // SB25_DRUM_MIDI_FUNCTIONS_H

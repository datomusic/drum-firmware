#ifndef SB25_DRUM_MIDI_FUNCTIONS_H
#define SB25_DRUM_MIDI_FUNCTIONS_H

#include <cstdint> // For uint8_t

// Forward declaration
namespace drum {
class SoundRouter;
} // namespace drum

// Function declarations (prototypes) for functions defined in midi.cpp

/**
 * @brief Initialize the MIDI system and callbacks.
 * @param sound_router Reference to the SoundRouter for handling MIDI note events.
 */
void midi_init(drum::SoundRouter &sound_router);

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

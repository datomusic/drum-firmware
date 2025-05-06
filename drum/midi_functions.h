#ifndef SB25_DRUM_MIDI_FUNCTIONS_H
#define SB25_DRUM_MIDI_FUNCTIONS_H

#include <cstdint> // For uint8_t

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

#endif // SB25_DRUM_MIDI_FUNCTIONS_H

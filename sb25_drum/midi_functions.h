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

// Note: send_midi_note and send_midi_cc are now primarily intended for internal
// use by SoundRouter. They are kept here as SoundRouter still needs to call them,
// but they are not intended for general use outside that context.
// Consider making them static within sound_router.cpp or part of a dedicated
// MIDI output class if further refactoring occurs.

/**
 * @brief Send a MIDI Control Change message.
 * @param channel MIDI channel (1-16).
 * @param cc_number Controller number (0-119).
 * @param value Controller value (0-127).
 */
void send_midi_cc(uint8_t channel, uint8_t cc_number, uint8_t value);

/**
 * @brief Send a MIDI Note On or Note Off message.
 * Note Off is typically sent as Note On with velocity 0.
 * @param channel MIDI channel (1-16).
 * @param note_number Note number (0-127).
 * @param velocity Note velocity (0-127). Velocity 0 usually means Note Off.
 */
void send_midi_note(uint8_t channel, uint8_t note_number, uint8_t velocity);
void send_midi_start();
void send_midi_stop();

#endif // SB25_DRUM_MIDI_FUNCTIONS_H

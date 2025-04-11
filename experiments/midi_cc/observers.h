// analog_control.h
// Contains MIDI-specific observer implementation
#ifndef EXPERIMENTS_MIDI_CC_ANALOG_CONTROL_OBSERVER_H
#define EXPERIMENTS_MIDI_CC_ANALOG_CONTROL_OBSERVER_H

#include "musin/ui/analog_control.h" // Include the base class from musin/ui
#include "musin/ui/keypad_hc138.h"
#include <cstdint>

/**
 * @brief MIDI CC observer implementation
 * Statically configured, no dynamic memory allocation.
 * This remains specific to the experiment.
 */
struct MIDICCObserver : public Musin::UI::AnalogControlObserverBase {
    const uint8_t cc_number;
    const uint8_t midi_channel;
    
    // Function pointer type defined elsewhere (e.g., main.cpp)
    using MIDISendFn = void (*)(uint8_t channel, uint8_t cc, uint8_t value);
    const MIDISendFn send_midi;
    
    // Constructor
    constexpr MIDICCObserver(uint8_t cc, uint8_t channel, MIDISendFn sender)
        : cc_number(cc), midi_channel(channel), send_midi(sender) {}
    
    void on_value_changed([[maybe_unused]] uint16_t control_id, float new_value, [[maybe_unused]] uint16_t raw_value) override {
        // Convert normalized value (0.0-1.0) to MIDI CC value (0-127)
        uint8_t cc_value = static_cast<uint8_t>(new_value * 127.0f);
        
        // Send MIDI CC message through function pointer
        send_midi(midi_channel, cc_number, cc_value);
    }
};

struct KeypadMIDICCMapObserver : public Musin::UI::KeypadObserverBase {
    const std::array<uint8_t, KEYPAD_TOTAL_KEYS>& _cc_map;
    const uint8_t _midi_channel;
    const MIDISendFn _send_midi;

    // Constructor
    constexpr KeypadMIDICCMapObserver(
        const std::array<uint8_t, KEYPAD_TOTAL_KEYS>& map,
        uint8_t channel,
        MIDISendFn sender)
        : _cc_map(map), _midi_channel(channel), _send_midi(sender) {}

    void on_key_pressed(uint8_t row, uint8_t col) override {
        uint8_t key_index = row * KEYPAD_COLS + col;
        if (key_index < _cc_map.size()) {
            uint8_t cc_number = _cc_map[key_index];
            if (cc_number != 0) { // Check if a valid CC was assigned
                 _send_midi(_midi_channel, cc_number, 100); // Send CC ON
            }
        }
    }

    void on_key_released(uint8_t row, uint8_t col) override {
         uint8_t key_index = row * KEYPAD_COLS + col;
        if (key_index < _cc_map.size()) {
            uint8_t cc_number = _cc_map[key_index];
            if (cc_number != 0) { // Check if a valid CC was assigned
                 _send_midi(_midi_channel, cc_number, 0); // Send CC OFF
            }
        }
    }

    void on_key_held(uint8_t row, uint8_t col) override {
      uint8_t key_index = row * KEYPAD_COLS + col;
      if (key_index < _cc_map.size()) {
          uint8_t cc_number = _cc_map[key_index];
          if (cc_number != 0) { // Check if a valid CC was assigned
               _send_midi(_midi_channel, cc_number, 127); // Send CC ON
          }
      }
    }
};

#endif // EXPERIMENTS_MIDI_CC_ANALOG_CONTROL_OBSERVER_H

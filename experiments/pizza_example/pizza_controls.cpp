#include "pizza_controls.h"
#include "pizza_display.h" // Need definition for display methods
#include "midi.h"          // For send_midi_cc, send_midi_note
#include <cstdio>          // For printf
#include <algorithm>       // For std::clamp
#include <cmath>           // For std::max used in scaling

// --- Constructor ---
PizzaControls::PizzaControls(PizzaDisplay& display_ref) :
    display(display_ref),
    keypad(keypad_decoder_pins, keypad_columns_pins, 10, 5, 1000),
    keypad_observer(this, keypad_cc_map, 0), // Pass parent pointer and map reference
    drumpad_readers{ // Initialize readers directly by calling constructors
        Musin::HAL::AnalogInMux16{PIN_ADC, analog_address_pins, DRUMPAD_ADDRESS_1},
        Musin::HAL::AnalogInMux16{PIN_ADC, analog_address_pins, DRUMPAD_ADDRESS_2},
        Musin::HAL::AnalogInMux16{PIN_ADC, analog_address_pins, DRUMPAD_ADDRESS_3},
        Musin::HAL::AnalogInMux16{PIN_ADC, analog_address_pins, DRUMPAD_ADDRESS_4}
    },
    drumpads{ // Initialize drumpads using the readers by calling constructors explicitly
        Musin::UI::Drumpad<Musin::HAL::AnalogInMux16>{drumpad_readers[0], 50U, 250U, 150U, 3000U, 100U, 800U, 1000U, 5000U, 200000U},
        Musin::UI::Drumpad<Musin::HAL::AnalogInMux16>{drumpad_readers[1], 50U, 250U, 150U, 3000U, 100U, 800U, 1000U, 5000U, 200000U},
        Musin::UI::Drumpad<Musin::HAL::AnalogInMux16>{drumpad_readers[2], 50U, 250U, 150U, 3000U, 100U, 800U, 1000U, 5000U, 200000U},
        Musin::UI::Drumpad<Musin::HAL::AnalogInMux16>{drumpad_readers[3], 50U, 250U, 150U, 3000U, 100U, 800U, 1000U, 5000U, 200000U}
    },
    drumpad_note_numbers{0, 7, 15, 23}, // Initial notes
    mux_controls{ // Initialize by explicitly calling constructors
        // Assuming order matches the enum in drum_pizza_hardware.h
        Musin::UI::AnalogControl{DRUM1,      PIN_ADC, analog_address_pins, DRUM1},
        Musin::UI::AnalogControl{FILTER,     PIN_ADC, analog_address_pins, FILTER},
        Musin::UI::AnalogControl{DRUM2,      PIN_ADC, analog_address_pins, DRUM2},
        Musin::UI::AnalogControl{PITCH1,     PIN_ADC, analog_address_pins, PITCH1},
        Musin::UI::AnalogControl{PITCH2,     PIN_ADC, analog_address_pins, PITCH2},
        Musin::UI::AnalogControl{PLAYBUTTON, PIN_ADC, analog_address_pins, PLAYBUTTON},
        Musin::UI::AnalogControl{RANDOM,     PIN_ADC, analog_address_pins, RANDOM},
        Musin::UI::AnalogControl{VOLUME,     PIN_ADC, analog_address_pins, VOLUME},
        Musin::UI::AnalogControl{PITCH3,     PIN_ADC, analog_address_pins, PITCH3},
        Musin::UI::AnalogControl{SWING,      PIN_ADC, analog_address_pins, SWING},
        Musin::UI::AnalogControl{CRUSH,      PIN_ADC, analog_address_pins, CRUSH},
        Musin::UI::AnalogControl{DRUM3,      PIN_ADC, analog_address_pins, DRUM3},
        Musin::UI::AnalogControl{REPEAT,     PIN_ADC, analog_address_pins, REPEAT},
        Musin::UI::AnalogControl{DRUM4,      PIN_ADC, analog_address_pins, DRUM4},
        Musin::UI::AnalogControl{SPEED,      PIN_ADC, analog_address_pins, SPEED},
        Musin::UI::AnalogControl{PITCH4,     PIN_ADC, analog_address_pins, PITCH4}
    },
    control_observers{ // Initialize observers by explicitly calling constructors
        InternalMIDICCObserver{this, DRUM1, 0},
        InternalMIDICCObserver{this, FILTER, 0},
        InternalMIDICCObserver{this, DRUM2, 0},
        InternalMIDICCObserver{this, PITCH1, 1},
        InternalMIDICCObserver{this, PITCH2, 2},
        InternalMIDICCObserver{this, PLAYBUTTON, 0},
        InternalMIDICCObserver{this, RANDOM, 0},
        InternalMIDICCObserver{this, VOLUME, 0},
        InternalMIDICCObserver{this, PITCH3, 3},
        InternalMIDICCObserver{this, SWING, 0},
        InternalMIDICCObserver{this, CRUSH, 0},
        InternalMIDICCObserver{this, DRUM3, 0},
        InternalMIDICCObserver{this, REPEAT, 0},
        InternalMIDICCObserver{this, DRUM4, 0},
        InternalMIDICCObserver{this, SPEED, 0},
        InternalMIDICCObserver{this, PITCH4, 4}
    }
{}

// --- Initialization ---
void PizzaControls::init() {
    printf("PizzaControls: Initializing...\n");

    // Initialize Keypad
    keypad.init();
    keypad.add_observer(keypad_observer);
    printf("PizzaControls: Keypad Initialized (%u rows, %u cols)\n", keypad.get_num_rows(), keypad.get_num_cols());

    // Initialize Drumpad Readers
    for (auto& reader : drumpad_readers) {
        reader.init();
    }
    // Drumpad objects themselves don't need init() as they use initialized readers
    printf("PizzaControls: Drumpad Readers Initialized\n");

    // Initialize Analog Controls and attach observers
    for (size_t i = 0; i < mux_controls.size(); ++i) {
        mux_controls[i].init();
        mux_controls[i].add_observer(control_observers[i]);
    }
    printf("PizzaControls: Initialized %zu analog controls\n", mux_controls.size());

    printf("PizzaControls: Initialization Complete.\n");
}

// --- Update ---
void PizzaControls::update() {
    // Update all analog mux controls - observers will be notified automatically
    for (auto &control : mux_controls) {
        control.update();
    }

    // Scan the keypad - observers will be notified automatically
    keypad.scan();

    // Update drumpads and handle MIDI/Display updates
    update_drumpads();

    // Display updates are requested within observers and update_drumpads
    // The actual display.show() is called in main.cpp's loop
}


// --- Color Utilities ---
// Assuming GRB order consistent with PizzaDisplay's WS2812 initialization
void PizzaControls::unpack_color(uint32_t packed_color, uint8_t& r, uint8_t& g, uint8_t& b) {
    g = (packed_color >> 16) & 0xFF;
    r = (packed_color >> 8) & 0xFF;
    b = packed_color & 0xFF;
}

uint32_t PizzaControls::pack_color(uint8_t r, uint8_t g, uint8_t b) {
    return (static_cast<uint32_t>(g) << 16) |
           (static_cast<uint32_t>(r) << 8) |
           b;
}

// --- Private Methods ---

float PizzaControls::scale_raw_to_brightness(uint16_t raw_value) const {
    // Map ADC range (e.g., 100-1000) to brightness (e.g., 0.1-1.0)
    // Adjust these based on sensor readings and desired visual response
    constexpr uint16_t min_adc = 100;
    constexpr uint16_t max_adc = 1000;
    constexpr float min_brightness = 0.1f;
    constexpr float max_brightness = 1.0f;

    if (raw_value <= min_adc) {
        return max_brightness; // Max brightness at minimum pressure (or below)
    }
    if (raw_value >= max_adc) {
        return 0.0f; // Off at maximum pressure (or above)
    }

    // Inverted linear scaling: factor decreases as raw_value increases
    float factor = static_cast<float>(max_adc - raw_value) / (max_adc - min_adc);
    return min_brightness + factor * (max_brightness - min_brightness); // Apply the inverted factor
}


uint32_t PizzaControls::calculate_brightness_color(uint32_t base_color, uint16_t raw_value) const {
    if (base_color == 0) return 0;

    uint8_t r, g, b;
    unpack_color(base_color, r, g, b);

    float brightness_factor = scale_raw_to_brightness(raw_value);

    r = static_cast<uint8_t>(std::clamp(static_cast<float>(r) * brightness_factor, 0.0f, 255.0f));
    g = static_cast<uint8_t>(std::clamp(static_cast<float>(g) * brightness_factor, 0.0f, 255.0f));
    b = static_cast<uint8_t>(std::clamp(static_cast<float>(b) * brightness_factor, 0.0f, 255.0f));

    return pack_color(r, g, b);
}


void PizzaControls::update_drumpads() {
    for (size_t i = 0; i < drumpads.size(); ++i) {
        bool state_changed = drumpads[i].update();
        uint16_t raw_value = drumpads[i].get_raw_adc_value();
        uint8_t note_index = drumpad_note_numbers[i];
        uint32_t led_index = display.get_drumpad_led_index(i);

        if (state_changed) {
            auto velocity = drumpads[i].get_velocity();
            uint8_t note_number = drumpad_note_numbers[i];
            uint8_t channel = i + 1; // MIDI channels 1-4

            if (drumpads[i].was_released()) {
                send_midi_note(channel, note_number, 0); // Send Note Off
            } else if (velocity) {
                // printf("Drum %u hit! Note: %u Velocity: %u (Raw: %u, TimeDiff: %llu us)\n", channel, note_number, *velocity, drumpads[i].get_raw_adc_value(), drumpads[i].get_last_velocity_time_diff());
                send_midi_note(channel, note_number, *velocity); // Send Note On
            }
        }

        // Update drumpad LED color based on selected note AND current pressure
        if (led_index < NUM_LEDS) {
             uint32_t base_color = display.get_note_color(note_index);
             uint32_t final_color = calculate_brightness_color(base_color, raw_value);
             display.set_led(led_index, final_color);
        }
    }
}

void PizzaControls::select_note_for_pad(uint8_t pad_index, int8_t offset) {
    if (pad_index >= drumpad_note_numbers.size()) return;

    int32_t current_note = drumpad_note_numbers[pad_index];
    int32_t new_note_number = current_note + offset;

    // Wrap around 0-31 range
    if (new_note_number < 0) {
        new_note_number = 31;
    } else if (new_note_number > 31) {
        new_note_number = 0;
    }
    drumpad_note_numbers[pad_index] = static_cast<uint8_t>(new_note_number);

    // Update the display for the affected pad immediately
    uint32_t led_index = display.get_drumpad_led_index(pad_index);
    if (led_index < NUM_LEDS) {
        uint32_t base_color = display.get_note_color(drumpad_note_numbers[pad_index]);
        // When selecting a note, show the base color at minimum brightness (or maybe full?)
        // Let's use calculate_brightness with a low raw value to show it's selected but not active.
        uint32_t final_color = calculate_brightness_color(base_color, 100); // Use min_adc value for base brightness
        display.set_led(led_index, final_color);
    }
}


// --- Observer Implementations ---

void PizzaControls::InternalMIDICCObserver::notification(Musin::UI::AnalogControlEvent event) {
    // Access parent members via parent pointer
    uint8_t value = static_cast<uint8_t>(event.value * 127.0f);

    switch (cc_number) {
        case PLAYBUTTON:
            // Update Play button LED via parent's display reference
            parent->display.set_play_button_led( (static_cast<uint32_t>(value*2) << 16) | (static_cast<uint32_t>(value*2) << 8) | (value*2) );
            break;
        // Drumpad cases removed as they didn't do anything specific here
        case SWING:
            // printf("Swing set to %d\n", value); // Keep printf if needed for debug
            send_midi_cc(1, cc_number, value); // Assuming channel 1 for general controls
            break;
        case PITCH1:
        case PITCH2:
        case PITCH3:
        case PITCH4:
            // Use the specific MIDI channel assigned in the observer's constructor
            send_midi_cc(midi_channel, cc_number, 127 - value);
            break;
        default:
            // Send other CCs on channel 1 (or adjust as needed)
            send_midi_cc(1, cc_number, value);
            break;
    }
}

void PizzaControls::InternalKeypadObserver::notification(Musin::UI::KeypadEvent event) {
    // Access parent members via parent pointer
    uint8_t key_index = (7 - event.row) * KEYPAD_COLS + event.col; // Use KEYPAD_COLS
    if (key_index >= cc_map.size()) return;

    // uint8_t cc_number = cc_map[key_index]; // CC number not used directly for MIDI here
    uint8_t value = 0; // Used for LED intensity and sample select logic

    switch (event.type) {
        case Musin::UI::KeypadEvent::Type::Press:
            value = 100;
            if (event.col == 4) { // Sample select buttons (rightmost column)
                // Map row to pad index (row 7 -> pad 0, row 0 -> pad 3)
                uint8_t pad_index = 0;
                int8_t offset = 0;
                switch (event.row) {
                    case 0: pad_index = 3; offset = -1; break; // Bottom pair -> Pad 4
                    case 1: pad_index = 3; offset =  1; break;
                    case 2: pad_index = 2; offset = -1; break; // Next pair -> Pad 3
                    case 3: pad_index = 2; offset =  1; break;
                    case 4: pad_index = 1; offset = -1; break; // Next pair -> Pad 2
                    case 5: pad_index = 1; offset =  1; break;
                    case 6: pad_index = 0; offset = -1; break; // Top pair -> Pad 1
                    case 7: pad_index = 0; offset =  1; break;
                }
                parent->select_note_for_pad(pad_index, offset);
            }
            break;
        case Musin::UI::KeypadEvent::Type::Release:
            value = 0;
            break;
        case Musin::UI::KeypadEvent::Type::Hold:
            // Hold doesn't seem to have specific behavior in original code other than LED
            value = 127;
            break;
    }

    // printf("Key %d %d\n", key_index, value); // Keep printf if needed

    // Update keypad LED via parent's display reference
    // Only update LEDs for sequencer columns (0-3)
    if (event.col < 4) {
        parent->display.set_keypad_led(event.row, event.col, value);
    }
}

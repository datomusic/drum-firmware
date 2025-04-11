// analog_control.h
#ifndef MUSIN_CONTROLLER_ANALOG_CONTROL_H
#define MUSIN_CONTROLLER_ANALOG_CONTROL_H

#include "musin/hal/analog_in.h"
#include <cstdint>
#include <array>
#include <algorithm>

namespace Musin::Controller {

// Forward declaration
template<uint8_t MaxObservers>
class AnalogControl;

/**
 * @brief Observer interface for analog control changes
 */
class AnalogControlObserverBase {
public:
    virtual ~AnalogControlObserverBase() = default;
    
    /**
     * @brief Called when an observed analog control value changes
     * 
     * @param control_id ID of the control that changed
     * @param new_value The new normalized value (0.0f to 1.0f)
     * @param raw_value The new raw ADC value
     */
    virtual void on_value_changed(uint16_t control_id, float new_value, uint16_t raw_value) = 0;
};

/**
 * @brief MIDI CC observer implementation
 * Statically configured, no dynamic memory allocation
 */
struct MIDICCObserver : public AnalogControlObserverBase {
    const uint8_t cc_number;
    const uint8_t midi_channel;
    
    // Function pointer to MIDI sender (no std::function)
    using MIDISendFn = void (*)(uint8_t channel, uint8_t cc, uint8_t value);
    const MIDISendFn send_midi;
    
    // Constructor
    constexpr MIDICCObserver(uint8_t cc, uint8_t channel, MIDISendFn sender)
        : cc_number(cc), midi_channel(channel), send_midi(sender) {}
    
    void on_value_changed(uint16_t control_id, float new_value, uint16_t raw_value) override {
        // Convert normalized value (0.0-1.0) to MIDI CC value (0-127)
        uint8_t cc_value = static_cast<uint8_t>(new_value * 127.0f);
        
        // Send MIDI CC message through function pointer
        send_midi(midi_channel, cc_number, cc_value);
    }
};

/**
 * @brief Represents a physical analog control (pot, fader, etc)
 * Using compile-time configuration and static allocation
 * 
 * @tparam MaxObservers Maximum number of observers for this control
 */
template<uint8_t MaxObservers = 1>
class AnalogControl {
public:
    /**
     * @brief Constructor for direct ADC pin connection
     * 
     * @param id Unique identifier for this control
     * @param adc_pin The GPIO pin number for ADC input
     * @param threshold Change threshold to trigger updates (normalized value)
     */
    AnalogControl(uint16_t id, uint32_t adc_pin, float threshold = 0.005f);
    
    /**
     * @brief Constructor for multiplexed ADC connection (8-channel)
     */
    AnalogControl(uint16_t id, uint32_t adc_pin, 
                 const std::array<std::uint32_t, 3>& mux_address_pins,
                 uint8_t mux_channel, float threshold = 0.005f);
    
    /**
     * @brief Constructor for multiplexed ADC connection (16-channel)
     */
    AnalogControl(uint16_t id, uint32_t adc_pin, 
                 const std::array<std::uint32_t, 4>& mux_address_pins,
                 uint8_t mux_channel, float threshold = 0.005f);
    
    /**
     * @brief Initialize the control's hardware
     */
    void init();
    
    /**
     * @brief Update the control's value
     * Reads the ADC, applies filtering, and notifies observers if value changed
     * 
     * @return true if value changed and observers were notified
     */
    bool update();
    
    /**
     * @brief Get the current normalized value (0.0f to 1.0f)
     */
    float get_value() const { return _current_value; }
    
    /**
     * @brief Get the current raw ADC value
     */
    uint16_t get_raw_value() const { return _current_raw; }
    
    /**
     * @brief Get the control's unique ID
     */
    uint16_t get_id() const { return _id; }
    
    /**
     * @brief Add an observer to be notified of value changes
     * @return true if observer added, false if array is full
     */
    bool add_observer(AnalogControlObserverBase* observer);
    
    /**
     * @brief Set the filtering coefficient
     * 
     * @param alpha Filter coefficient (0.0f = heavy filtering, 1.0f = no filtering)
     */
    void set_filter_coefficient(float alpha) { _filter_alpha = std::clamp(alpha, 0.0f, 1.0f); }
    
    /**
     * @brief Set the change threshold
     * 
     * @param threshold Minimum change in normalized value to trigger an update
     */
    void set_threshold(float threshold) { _threshold = threshold; }
    
private:
    // Control identification
    uint16_t _id;
    
    // Value tracking
    float _current_value = 0.0f;
    float _filtered_value = 0.0f;
    uint16_t _current_raw = 0;
    float _threshold;
    float _filter_alpha = 0.3f; // Default filter strength
    
    // Hardware abstraction - use direct instances, not pointers
    enum class InputType { Direct, Mux8, Mux16 };
    InputType _input_type;
    
    union {
        Musin::HAL::AnalogIn _analog_in;
        Musin::HAL::AnalogInMux8 _mux8;
        Musin::HAL::AnalogInMux16 _mux16;
    };
    
    // Observer array with fixed maximum size
    AnalogControlObserverBase* _observers[MaxObservers];
    uint8_t _observer_count = 0;
    
    // Internal methods
    void notify_observers();
    void read_input();

    // Value tracking
    float _last_notified_value = -1.0f; // Store the last value that triggered a notification
};

} // namespace Musin::Controller

// // Include implementation for template class
#include "analog_control.cpp"

#endif // MUSIN_CONTROLLER_ANALOG_CONTROL_H

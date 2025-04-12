#ifndef MUSIN_UI_ANALOG_CONTROL_H
#define MUSIN_UI_ANALOG_CONTROL_H

#include "musin/hal/analog_in.h"
#include <cstdint>
#include <array>
#include <algorithm> // For std::clamp
#include "etl/observer.h" // Include ETL observer pattern

namespace Musin::UI {

/**
 * @brief Event data structure for analog control notifications
 */
struct AnalogControlEvent {
    uint16_t control_id;
    float value;
    uint16_t raw_value;
};

/**
 * @brief Represents a physical analog control (pot, fader, etc)
 * Using compile-time configuration and static allocation
 */
class AnalogControl : public etl::observable<etl::observer<AnalogControlEvent>, 4> {
public:
    template <int AddressPinCount>struct Config {
      static_assert(AddressPinCount == 0 ||AddressPinCount == 3 || AddressPinCount == 4);

      const std::array<std::uint32_t, AddressPinCount>& mux_address_pins;
      const uint32_t adc_pin;
      const uint8_t mux_channel; 
      const float threshold = 0.005f;

      const Config with_channel(const uint8_t channel) const {
        return Config{
          .mux_address_pins = mux_address_pins,
          .adc_pin = adc_pin,
          .mux_channel = channel,
          .threshold = threshold
        };
      }
    };

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

    template <int AddressPinCount>AnalogControl(uint16_t id, const Config<AddressPinCount> &config){
      AnalogControl(id, config.adc_pin, config.mux_address_pins, config.mux_channel, config.threshold);
    }
    
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
    
    
    // Internal methods
    void read_input();

    // Value tracking
    float _last_notified_value = -1.0f; // Store the last value that triggered a notification
};

// --- Template Implementation ---
// Implementation is included directly in the header for template classes

AnalogControl::AnalogControl(uint16_t id, uint32_t adc_pin, float threshold)
    : _id(id), 
      _threshold(threshold),
      _input_type(InputType::Direct),
      _analog_in(adc_pin) { // Construct directly in the union member
}

AnalogControl::AnalogControl(uint16_t id, uint32_t adc_pin, 
                            const std::array<std::uint32_t, 3>& mux_address_pins,
                            uint8_t mux_channel, float threshold)
    : _id(id), 
      _threshold(threshold),
      _input_type(InputType::Mux8) {
    
    // Use placement new to construct the mux in the union
    new (&_mux8) Musin::HAL::AnalogInMux8(adc_pin, mux_address_pins, mux_channel);
}

AnalogControl::AnalogControl(uint16_t id, uint32_t adc_pin, 
                            const std::array<std::uint32_t, 4>& mux_address_pins,
                            uint8_t mux_channel, float threshold)
    : _id(id), 
      _threshold(threshold),
      _input_type(InputType::Mux16) {
    
    // Use placement new to construct the mux in the union
    new (&_mux16) Musin::HAL::AnalogInMux16(adc_pin, mux_address_pins, mux_channel);
}

void AnalogControl::init() {
    // Initialize the appropriate hardware
    switch (_input_type) {
        case InputType::Direct:
            _analog_in.init();
            break;
        case InputType::Mux8:
            _mux8.init();
            break;
        case InputType::Mux16:
            _mux16.init();
            break;
    }
    // Initialize filtered value to the first reading to avoid large initial jump
    read_input(); 
    _last_notified_value = _current_value; 
}

void AnalogControl::read_input() {
    float raw_normalized = 0.0f;
    
    // Read from either direct ADC or multiplexer
    switch (_input_type) {
        case InputType::Direct:
            raw_normalized = _analog_in.read();
            _current_raw = _analog_in.read_raw();
            break;
        case InputType::Mux8:
            raw_normalized = _mux8.read();
            _current_raw = _mux8.read_raw();
            break;
        case InputType::Mux16:
            raw_normalized = _mux16.read();
            _current_raw = _mux16.read_raw();
            break;
    }
    
    // Apply filtering using a simple IIR low-pass filter
    // Make sure _filtered_value is initialized properly (e.g., in init() or constructor)
    _filtered_value = (_filter_alpha * raw_normalized) + 
                     ((1.0f - _filter_alpha) * _filtered_value);
    
    // Update current value (which represents the filtered value)
    _current_value = _filtered_value;
}

bool AnalogControl::update() {
    // Read and filter input - this updates _current_value
    read_input();
    
    // Check if the filtered value changed beyond threshold compared to the last notified value
    if (std::abs(_current_value - _last_notified_value) > _threshold) {
        this->notify_observers(AnalogControlEvent{_id, _current_value, _current_raw});
        _last_notified_value = _current_value; // Update the last notified value
        return true;
    }
    return false;
}



// Explicit template instantiation can be added here if needed for specific MaxObservers values
// e.g., template class AnalogControl<1>; 
//      template class AnalogControl<4>;
// This helps with compile times if you know the common sizes you'll use,
// but requires the implementation to be visible (which it now is).

} // namespace Musin::UI

#endif // MUSIN_UI_ANALOG_CONTROL_H

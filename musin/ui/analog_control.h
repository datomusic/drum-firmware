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

// Forward declaration
class AnalogControl;


/**
 * @brief Represents a physical analog control (pot, fader, etc)
 * Using compile-time configuration and static allocation
 */
class AnalogControl : public etl::observable<etl::observer<AnalogControlEvent>, 4> {
public:
    /**
     * @brief Constructor for direct ADC pin connection
     * 
     * @param adc_pin The GPIO pin number for ADC input
     * @param threshold Change threshold to trigger updates (normalized value)
     * @param invert If true, map raw 0.0->1.0 to 1.0->0.0
     */
    explicit AnalogControl(uint32_t adc_pin, float threshold = 0.005f, bool invert = false);
    
    /**
     * @brief Constructor for multiplexed ADC connection (8-channel)
     */
    AnalogControl(uint32_t adc_pin, 
                 const std::array<std::uint32_t, 3>& mux_address_pins,
                 uint8_t mux_channel, float threshold = 0.005f, bool invert = false);
    
    /**
     * @brief Constructor for multiplexed ADC connection (16-channel)
     */
    AnalogControl(uint32_t adc_pin, 
                 const std::array<std::uint32_t, 4>& mux_address_pins,
                 uint8_t mux_channel, float threshold = 0.005f, bool invert = false);
    
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
    bool _invert_mapping; // Flag to invert the 0.0-1.0 mapping
    
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

// Implementation details are moved to the .cpp file

} // namespace Musin::UI

#endif // MUSIN_UI_ANALOG_CONTROL_H

// analog_control.cpp
// This file contains the implementation of the template class
// Include this at the end of the header file
#include "analog_control.h"

namespace Musin::Controller {

template<uint8_t MaxObservers>
AnalogControl<MaxObservers>::AnalogControl(uint16_t id, uint32_t adc_pin, float threshold)
    : _id(id), 
      _threshold(threshold),
      _input_type(InputType::Direct),
      _analog_in(adc_pin) {
    
    // Initialize observer array
    for (uint8_t i = 0; i < MaxObservers; i++) {
        _observers[i] = nullptr;
    }
}

template<uint8_t MaxObservers>
AnalogControl<MaxObservers>::AnalogControl(uint16_t id, uint32_t adc_pin, 
                                          const std::array<std::uint32_t, 3>& mux_address_pins,
                                          uint8_t mux_channel, float threshold)
    : _id(id), 
      _threshold(threshold),
      _input_type(InputType::Mux8) {
    
    // Use placement new to construct the mux in the union
    // Pass the std::array directly
    new (&_mux8) Musin::HAL::AnalogInMux8(adc_pin, mux_address_pins, mux_channel);
    
    // Initialize observer array
    for (uint8_t i = 0; i < MaxObservers; i++) {
        _observers[i] = nullptr;
    }
}

template<uint8_t MaxObservers>
AnalogControl<MaxObservers>::AnalogControl(uint16_t id, uint32_t adc_pin, 
                                          const std::array<std::uint32_t, 4>& mux_address_pins,
                                          uint8_t mux_channel, float threshold)
    : _id(id), 
      _threshold(threshold),
      _input_type(InputType::Mux16) {
    
    // Use placement new to construct the mux in the union
    // Pass the std::array directly
    new (&_mux16) Musin::HAL::AnalogInMux16(adc_pin, mux_address_pins, mux_channel);
    
    // Initialize observer array
    for (uint8_t i = 0; i < MaxObservers; i++) {
        _observers[i] = nullptr;
    }
}

template<uint8_t MaxObservers>
void AnalogControl<MaxObservers>::init() {
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
}

template<uint8_t MaxObservers>
void AnalogControl<MaxObservers>::read_input() {
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
    
    // Apply filtering
    _filtered_value = (_filter_alpha * raw_normalized) + 
                     ((1.0f - _filter_alpha) * _filtered_value);
    
    // Update current value
    _current_value = _filtered_value;
}

template<uint8_t MaxObservers>
bool AnalogControl<MaxObservers>::update() {
    // Remember previous value
    float previous_value = _current_value;
    
    // Read and filter input
    read_input();
    
    // Check if value changed beyond threshold
    if (std::abs(_current_value - previous_value) > _threshold) {
        notify_observers();
        return true;
    }
    
    return false;
}

template<uint8_t MaxObservers>
bool AnalogControl<MaxObservers>::add_observer(AnalogControlObserverBase* observer) {
    if (!observer) {
        return false;
    }
    
    // Check if we have space
    if (_observer_count >= MaxObservers) {
        return false;
    }
    
    // Add to the array
    _observers[_observer_count++] = observer;
    return true;
}

template<uint8_t MaxObservers>
void AnalogControl<MaxObservers>::notify_observers() {
    for (uint8_t i = 0; i < _observer_count; i++) {
        if (_observers[i]) {
            _observers[i]->on_value_changed(_id, _current_value, _current_raw);
        }
    }
}

} // namespace Musin::Controller

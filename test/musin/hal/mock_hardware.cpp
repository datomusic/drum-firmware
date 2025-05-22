#include "mock_hardware.h"
#include <cassert>
#include <chrono>
#include <iostream>

namespace mock {

// Initialize global state
std::map<uint32_t, GpioState> gpio_states;
AdcState adc_state;
bool hardware_initialized = false;
uint64_t current_time_us = 1000000; // Start at 1 second

void reset_hardware_state() {
    gpio_states.clear();
    adc_state = AdcState{};
    hardware_initialized = false;
    current_time_us = 1000000;
}

} // namespace mock

// Mock implementations of hardware functions
extern "C" {

// GPIO functions
void gpio_init(uint32_t pin) {
    mock::gpio_states[pin].initialized = true;
}

void gpio_set_dir(uint32_t pin, uint8_t dir) {
    assert(mock::gpio_states[pin].initialized);
    mock::gpio_states[pin].direction = dir;
}

void gpio_put(uint32_t pin, bool value) {
    assert(mock::gpio_states[pin].initialized);
    assert(mock::gpio_states[pin].direction == mock::GPIO_OUT);
    mock::gpio_states[pin].value = value;
}

bool gpio_get(uint32_t pin) {
    assert(mock::gpio_states[pin].initialized);
    return mock::gpio_states[pin].value;
}

// ADC functions
void adc_init() {
    mock::adc_state.initialized = true;
}

void adc_gpio_init(uint32_t pin) {
    assert(pin >= 26 && pin <= 29); // Valid ADC pins
    gpio_init(pin);
    gpio_set_dir(pin, mock::GPIO_IN);
}

void adc_select_input(uint8_t input) {
    assert(mock::adc_state.initialized);
    assert(input <= 3); // Valid ADC channels
    mock::adc_state.selected_input = input;
}

uint16_t adc_read() {
    assert(mock::adc_state.initialized);
    return mock::adc_state.channel_values[mock::adc_state.selected_input];
}

void adc_set_temp_sensor_enabled(bool enabled) {
    assert(mock::adc_state.initialized);
    mock::adc_state.temp_sensor_enabled = enabled;
}

// Time functions
void busy_wait_us(uint32_t us) {
    // In mock, we just advance the time
    mock::current_time_us += us;
}

// Assertion function
void hard_assert(bool condition) {
    assert(condition);
}

// Time type and functions
absolute_time_t get_absolute_time() {
    return mock::current_time_us;
}

bool time_reached(absolute_time_t t) {
    return mock::current_time_us >= t;
}

int64_t absolute_time_diff_us(absolute_time_t from, absolute_time_t to) {
    return static_cast<int64_t>(to - from);
}

absolute_time_t delayed_by_us(absolute_time_t t, uint64_t us) {
    return t + us;
}

absolute_time_t delayed_by_ms(absolute_time_t t, uint32_t ms) {
    return t + (static_cast<uint64_t>(ms) * 1000);
}

} // extern "C"
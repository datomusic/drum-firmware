#ifndef MOCK_HARDWARE_H
#define MOCK_HARDWARE_H

#include <cstdint>
#include <map>
#include <vector>

// Mock implementation of hardware functions used by the HAL layer
// This allows testing without actual hardware

namespace mock {

// GPIO direction constants
constexpr uint8_t GPIO_IN = 0;
constexpr uint8_t GPIO_OUT = 1;

// Mock GPIO state
struct GpioState {
    bool initialized = false;
    uint8_t direction = GPIO_IN;
    bool value = false;
};

// Mock ADC state
struct AdcState {
    bool initialized = false;
    bool temp_sensor_enabled = false;
    uint8_t selected_input = 0;
    std::map<uint8_t, uint16_t> channel_values;
};

// Global state for mocking
extern std::map<uint32_t, GpioState> gpio_states;
extern AdcState adc_state;
extern bool hardware_initialized;

// Reset all mock state to defaults
void reset_hardware_state();

} // namespace mock

// Mock implementations of hardware functions
extern "C" {

// GPIO functions
void gpio_init(uint32_t pin);
void gpio_set_dir(uint32_t pin, uint8_t dir);
void gpio_put(uint32_t pin, bool value);
bool gpio_get(uint32_t pin);

// ADC functions
void adc_init();
void adc_gpio_init(uint32_t pin);
void adc_select_input(uint8_t input);
uint16_t adc_read();
void adc_set_temp_sensor_enabled(bool enabled);

// Time functions
void busy_wait_us(uint32_t us);

// Assertion function
void hard_assert(bool condition);

// Time type and functions
typedef uint64_t absolute_time_t;
constexpr absolute_time_t nil_time = 0;
absolute_time_t get_absolute_time();
bool time_reached(absolute_time_t t);
int64_t absolute_time_diff_us(absolute_time_t from, absolute_time_t to);
absolute_time_t delayed_by_us(absolute_time_t t, uint64_t us);
absolute_time_t delayed_by_ms(absolute_time_t t, uint32_t ms);

} // extern "C"

#endif // MOCK_HARDWARE_H
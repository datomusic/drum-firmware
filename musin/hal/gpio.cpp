#include "gpio.h"

// Include necessary Pico SDK headers for implementation
extern "C" {
#include "hardware/gpio.h"
}

namespace Musin::HAL {

GpioPin::GpioPin(std::uint32_t pin) : _pin(pin) {
    gpio_init(_pin);
}

void GpioPin::set_direction(GpioDirection dir) {
    gpio_set_dir(_pin, static_cast<bool>(dir));
}

void GpioPin::write(bool value) {
    gpio_put(_pin, value);
}

bool GpioPin::read() const {
    return gpio_get(_pin);
}

void GpioPin::enable_pullup() {
    gpio_pull_up(_pin);
}

void GpioPin::enable_pulldown() {
    gpio_pull_down(_pin);
}

void GpioPin::disable_pulls() {
    gpio_disable_pulls(_pin);
}

} // namespace Musin::HAL

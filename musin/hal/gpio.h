#ifndef MUSIN_HAL_GPIO_H
#define MUSIN_HAL_GPIO_H

#include <cstdint>

// Forward declare Pico SDK types if needed, or include minimal headers
// For simplicity here, we assume SDK types like 'uint' are available
// or we use standard types like std::uint32_t directly.

namespace Musin::HAL {

enum class GpioDirection : bool {
    IN = false,
    OUT = true
};

class GpioPin {
public:
    explicit GpioPin(std::uint32_t pin);

    // Prevent copying/moving for simplicity and RAII
    GpioPin(const GpioPin&) = delete;
    GpioPin& operator=(const GpioPin&) = delete;
    GpioPin(GpioPin&&) = delete;
    GpioPin& operator=(GpioPin&&) = delete;

    void set_direction(GpioDirection dir);
    void write(bool value);
    bool read() const;
    void enable_pullup();
    void enable_pulldown();
    void disable_pulls();

private:
    const std::uint32_t _pin;
};

} // namespace Musin::HAL

#endif // MUSIN_HAL_GPIO_H

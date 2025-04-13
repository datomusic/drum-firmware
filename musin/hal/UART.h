#ifndef MUSIN_HAL_UART_H
#define MUSIN_HAL_UART_H

#include <cstdint>
#include <cstddef>
#include "hardware/uart.h"
#include "hardware/gpio.h"

namespace Musin::HAL {

namespace detail {

// --- Platform-specific Pin Validation Helpers ---

#if PICO_RP2040
constexpr bool is_valid_uart0_tx(std::uint32_t pin) {
    switch (pin) { case 0: case 12: case 16: case 28: return true; default: return false; }
}
constexpr bool is_valid_uart0_rx(std::uint32_t pin) {
    switch (pin) { case 1: case 13: case 17: case 29: return true; default: return false; }
}
constexpr bool is_valid_uart1_tx(std::uint32_t pin) {
    switch (pin) { case 4: case 8: case 20: case 24: return true; default: return false; }
}
constexpr bool is_valid_uart1_rx(std::uint32_t pin) {
    switch (pin) { case 5: case 9: case 21: case 25: return true; default: return false; }
}
#elif PICO_RP2350
constexpr bool is_valid_uart0_tx(std::uint32_t pin) {
    switch (pin) {
        case 0: case 2: case 12: case 14: case 16: case 18: case 28: case 30:
        case 32: case 34: case 46: return true;
        default: return false;
    }
}
constexpr bool is_valid_uart0_rx(std::uint32_t pin) {
    switch (pin) {
        case 1: case 3: case 13: case 15: case 17: case 19: case 29: case 31:
        case 33: case 35: case 47: return true;
        default: return false;
    }
}
constexpr bool is_valid_uart1_tx(std::uint32_t pin) {
    switch (pin) {
        case 4: case 6: case 8: case 10: case 20: case 22: case 24: case 26:
        case 36: case 38: case 40: case 42: return true;
        default: return false;
    }
}
constexpr bool is_valid_uart1_rx(std::uint32_t pin) {
    switch (pin) {
        case 5: case 7: case 9: case 11: case 21: case 23: case 25: case 27:
        case 37: case 39: case 41: case 43: return true;
        default: return false;
    }
}
#else
    #error "Unsupported target platform for UART HAL pin validation"
    // Define dummy functions to allow compilation, but they will always fail validation
    constexpr bool is_valid_uart0_tx(std::uint32_t) { return false; }
    constexpr bool is_valid_uart0_rx(std::uint32_t) { return false; }
    constexpr bool is_valid_uart1_tx(std::uint32_t) { return false; }
    constexpr bool is_valid_uart1_rx(std::uint32_t) { return false; }
#endif

// --- Combined Validation Function ---

// Constexpr function to determine the UART index for a given TX/RX pin pair.
// Returns 0 if the pair is valid for UART0, 1 if valid for UART1,
// and -1 if the pair is invalid for either UART.
constexpr int get_uart_index(std::uint32_t tx_pin, std::uint32_t rx_pin) {
    if (is_valid_uart0_tx(tx_pin) && is_valid_uart0_rx(rx_pin)) {
        return 0;
    }
    if (is_valid_uart1_tx(tx_pin) && is_valid_uart1_rx(rx_pin)) {
        return 1;
    }
    return -1; // Invalid combination for any UART
}

// Helper to get the uart_inst_t* based on the validated index
constexpr uart_inst_t* get_uart_instance_from_index(int index) {
    #if PICO_RP2040 || PICO_RP2350 // Assuming uart0/uart1 exist on RP2350 too
        return (index == 0) ? uart0 : uart1;
    #else
        return nullptr; // Should be caught by #error earlier
    #endif
}


} // namespace detail


/**
 * @brief Provides a hardware abstraction layer for UART communication
 *        on the RP2040, with compile-time pin validation.
 *
 * This class wraps the Pico SDK UART functions. The TX and RX pins are specified
 * as template parameters and validated at compile time.
 *
 * @tparam TxPin The GPIO pin number for the UART TX line.
 * @tparam RxPin The GPIO pin number for the UART RX line.
 */
template <std::uint32_t TxPin, std::uint32_t RxPin>
class UART {
public:
  // Compile-time validation: Ensure pins form a valid pair for either UART0 or UART1.
  static_assert(detail::get_uart_index(TxPin, RxPin) != -1,
                "Invalid TX/RX pins: Must be a valid TX/RX pair for the same UART instance (UART0 or UART1).");

  /**
   * @brief Default constructor. The UART is not initialized.
   */
  UART() : _initialized(false) {}

  /**
   * @brief Initializes the UART peripheral associated with the template pins.
   *
   * Configures the pins and the UART peripheral.
   *
   * @param baud_rate The desired baud rate in Hz.
   * @return true (initialization always succeeds if compilation passed).
   */
  bool init(std::uint32_t baud_rate) {
    // Get the UART index (guaranteed valid by static_assert)
    constexpr int uart_index = detail::get_uart_index(TxPin, RxPin);
    uart_inst_t* uart_instance = detail::get_uart_instance_from_index(uart_index);

    // Initialize UART
    uart_init(uart_instance, baud_rate);

    // Set the GPIO pin muxing
    gpio_set_function(TxPin, GPIO_FUNC_UART);
    gpio_set_function(RxPin, GPIO_FUNC_UART);

    _initialized = true;
    return true;
  }


  /**
   * @brief Reads a single byte from the UART receive buffer.
   *
   *
   * This is a blocking call if no data is available. Check `available()` first
   * for non-blocking reads.
   *
   * @return The byte read from the UART. Returns 0 if not initialized.
   */
  std::uint8_t read() {
    if (!_initialized) {
      return 0;
    }
    // uart_getc is blocking, waits for character
    constexpr int uart_index = detail::get_uart_index(TxPin, RxPin);
    return uart_getc(detail::get_uart_instance_from_index(uart_index));
  }

  /**
   * @brief Writes a single byte to the UART transmit buffer.
   *
   *
   * This may block if the transmit buffer is full.
   *
   * @param byte The byte to write.
   * @return The number of bytes written (always 1 if successful, 0 if not initialized).
   */
  size_t write(std::uint8_t byte) {
    if (!_initialized) {
      return 0;
    }
    // uart_putc is blocking, waits for space in FIFO
    constexpr int uart_index = detail::get_uart_index(TxPin, RxPin);
    uart_putc(detail::get_uart_instance_from_index(uart_index), byte);
    return 1; // Indicate one byte was written
  }

  /**
   * @brief Checks if there is data available to read from the UART.
   * @return true if data is available, false otherwise or if not initialized.
   */
  bool available() {
    if (!_initialized) {
      return false;
    }
    constexpr int uart_index = detail::get_uart_index(TxPin, RxPin);
    return uart_is_readable(detail::get_uart_instance_from_index(uart_index));
  }

private:
  bool _initialized;
};

} // namespace Musin::HAL

#endif // MUSIN_HAL_UART_H

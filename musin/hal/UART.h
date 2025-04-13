#ifndef MUSIN_HAL_UART_H
#define MUSIN_HAL_UART_H

#include <cstdint>
#include <cstddef>
#include "hardware/uart.h"
#include "hardware/gpio.h"

namespace Musin::HAL {

namespace detail {
// Constexpr function to check if pins are valid for any UART instance
constexpr bool are_pins_valid_uart(std::uint32_t tx_pin, std::uint32_t rx_pin) {
  #if PICO_RP2040
    return (((tx_pin == 0) && (rx_pin == 1)) || ((tx_pin == 12) && (rx_pin == 13)) ||
            ((tx_pin == 16) && (rx_pin == 17)) || ((tx_pin == 28) && (rx_pin == 29)) || // UART0
            ((tx_pin == 4) && (rx_pin == 5)) || ((tx_pin == 8) && (rx_pin == 9)) ||
            ((tx_pin == 20) && (rx_pin == 21)) || ((tx_pin == 24) && (rx_pin == 25))); // UART1
  #elif PICO_RP2350
    
  #endif
}

// Constexpr function to get the UART instance pointer for validated pins
constexpr uart_inst_t* get_validated_uart_instance(std::uint32_t tx_pin, std::uint32_t rx_pin) {
     // Assumes are_pins_valid_uart has already passed via static_assert
     if (((tx_pin == 0) && (rx_pin == 1)) || ((tx_pin == 12) && (rx_pin == 13)) ||
        ((tx_pin == 16) && (rx_pin == 17)) || ((tx_pin == 28) && (rx_pin == 29))) {
        return uart0;
    } else { // Must be UART1 if validation passed
        return uart1;
    }
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
  // Compile-time validation: Ensure pins are valid and belong to the same UART instance.
  static_assert(detail::get_uart_instance(TxPin, RxPin) != nullptr,
                "Invalid TX/RX pins: Must be valid UART pins and belong to the same UART instance.");

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
    // Get the validated UART instance (guaranteed non-nullptr due to static_assert)
    uart_inst_t* uart_instance = detail::get_uart_instance(TxPin, RxPin);

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
    return uart_getc(detail::get_uart_instance(TxPin, RxPin));
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
    uart_putc(detail::get_uart_instance(TxPin, RxPin), byte);
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
    return uart_is_readable(detail::get_uart_instance(TxPin, RxPin));
  }

private:
  bool _initialized;
};

} // namespace Musin::HAL

#endif // MUSIN_HAL_UART_H

#ifndef MUSIN_HAL_UART_H
#define MUSIN_HAL_UART_H

#include <cstdint>
#include <cstddef>
#include "hardware/uart.h"
#include "hardware/gpio.h"

namespace Musin::HAL {

namespace detail {

// Constexpr function to determine the UART index for a given pin.
// Returns 0 for UART0, 1 for UART1, -1 if the pin is not a valid UART pin.
constexpr int get_uart_index(std::uint32_t pin) {
  #if PICO_RP2040
    switch (pin) {
        // UART0 Pins
        case 0: case 1: case 12: case 13: case 16: case 17: case 28: case 29:
            return 0;
        // UART1 Pins
        case 4: case 5: case 8: case 9: case 20: case 21: case 24: case 25:
            return 1;
        default:
            return -1; // Not a valid UART pin
    }
  #elif PICO_RP2350
    switch (pin) {
        // UART0 Pins (TX: 0, 2, 12, 14, 16, 18, 28, 30, 32, 34, 46 | RX: 1, 3, 13, 15, 17, 19, 29, 31, 33, 35, 47)
        case 0: case 1: case 2: case 3: case 12: case 13: case 14: case 15: case 16: case 17:
        case 18: case 19: case 28: case 29: case 30: case 31: case 32: case 33: case 34: case 35:
        case 46: case 47:
            return 0;
        // UART1 Pins (TX: 4, 6, 8, 10, 20, 22, 24, 26, 36, 38, 40, 42 | RX: 5, 7, 9, 11, 21, 23, 25, 27, 37, 39, 41, 43)
        case 4: case 5: case 6: case 7: case 8: case 9: case 10: case 11: case 20: case 21:
        case 22: case 23: case 24: case 25: case 26: case 27: case 36: case 37: case 38: case 39:
        case 40: case 41: case 42: case 43:
            return 1;
        default:
            return -1; // Not a valid UART pin for RP2350
    }
  #else
    #error "Unsupported target platform for UART HAL"
    return -1;
  #endif
}


// Constexpr function to get the UART instance if TX and RX pins are valid
// and belong to the same UART peripheral. Returns nullptr otherwise.
constexpr uart_inst_t* get_uart_instance(std::uint32_t tx_pin, std::uint32_t rx_pin) {
    const int tx_uart_index = get_uart_index(tx_pin);
    const int rx_uart_index = get_uart_index(rx_pin);

    // Check if both pins are valid and belong to the same UART instance
    if (tx_uart_index != -1 && rx_uart_index != -1 && tx_uart_index == rx_uart_index) {
        #if PICO_RP2040 || PICO_RP2350 // Assuming uart0/uart1 exist on RP2350 too
        return (tx_uart_index == 0) ? uart0 : uart1;
        #else
        return nullptr; // Should be caught by #error in get_uart_index
        #endif
    } else {
        return nullptr; // Pins are invalid or belong to different UARTs
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

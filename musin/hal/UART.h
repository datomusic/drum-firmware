#ifndef MUSIN_HAL_UART_H
#define MUSIN_HAL_UART_H

#include "etl/array.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace musin::hal {

namespace detail {

// --- Platform-specific Pin Definitions ---

#if PICO_RP2040
// UART0 valid pins
constexpr etl::array<std::uint32_t, 4> uart0_tx_pins = {0, 12, 16, 28};
constexpr etl::array<std::uint32_t, 4> uart0_rx_pins = {1, 13, 17, 29};
// UART1 valid pins
constexpr etl::array<std::uint32_t, 4> uart1_tx_pins = {4, 8, 20, 24};
constexpr etl::array<std::uint32_t, 4> uart1_rx_pins = {5, 9, 21, 25};

#elif PICO_RP2350
// UART0 valid pins (TX)
constexpr etl::array<std::uint32_t, 11> uart0_tx_pins = {0,  2,  12, 14, 16, 18,
                                                         28, 30, 32, 34, 46};
// UART0 valid pins (RX)
constexpr etl::array<std::uint32_t, 11> uart0_rx_pins = {1,  3,  13, 15, 17, 19,
                                                         29, 31, 33, 35, 47};
// UART1 valid pins (TX)
constexpr etl::array<std::uint32_t, 12> uart1_tx_pins = {
    4, 6, 8, 10, 20, 22, 24, 26, 36, 38, 40, 42};
// UART1 valid pins (RX)
constexpr etl::array<std::uint32_t, 12> uart1_rx_pins = {
    5, 7, 9, 11, 21, 23, 25, 27, 37, 39, 41, 43};
#else
#error "Unsupported target platform for UART HAL pin validation"
#endif

// Enum to identify UART instances in a constexpr-friendly way
enum class UartId {
  UART0,
  UART1,
  NONE
};

// Generic pin checker
template <std::uint32_t Pin, const auto &ValidPins> constexpr bool check_pin() {
  for (size_t i = 0; i < ValidPins.size(); ++i) {
    if (ValidPins[i] == Pin)
      return true;
  }
  return false;
}

// UART instance determination
template <std::uint32_t Tx, std::uint32_t Rx, typename = void>
struct uart_instance {
  static constexpr UartId value = UartId::NONE;
};

// UART0 specialization
template <std::uint32_t Tx, std::uint32_t Rx>
struct uart_instance<Tx, Rx,
                     std::enable_if_t<check_pin<Tx, uart0_tx_pins>() &&
                                      check_pin<Rx, uart0_rx_pins>()>> {
  static constexpr UartId value = UartId::UART0;
};

// UART1 specialization
template <std::uint32_t Tx, std::uint32_t Rx>
struct uart_instance<Tx, Rx,
                     std::enable_if_t<check_pin<Tx, uart1_tx_pins>() &&
                                      check_pin<Rx, uart1_rx_pins>()>> {
  static constexpr UartId value = UartId::UART1;
};

} // namespace detail

/**
 * @brief Provides a hardware abstraction layer for UART communication on the
 *        Pico family or microcontrollers, with compile-time pin validation.
 *
 * This class wraps the Pico SDK UART functions. The TX and RX pins are
 * specified as template parameters and validated at compile time.
 *
 * @tparam TxPin The GPIO pin number for the UART TX line.
 * @tparam RxPin The GPIO pin number for the UART RX line.
 */
template <std::uint32_t TxPin, std::uint32_t RxPin> class UART {
public:
  // Compile-time validation: Ensure pins form a valid pair for either UART0 or
  // UART1.
  static constexpr auto uart_id = detail::uart_instance<TxPin, RxPin>::value;

  static_assert(uart_id != detail::UartId::NONE,
                "Invalid TX/RX pins: Must be a valid TX/RX pair for the same "
                "UART instance (UART0 or UART1).");

  /**
   * @brief Default constructor. The UART is not initialized.
   */
  UART() : _initialized(false) {
  }

  /**
   * @brief Initializes the UART peripheral associated with the template pins.
   *
   * Configures the pins and the UART peripheral. This method is compatible
   * with the Arduino MIDI library.
   *
   * @param baud_rate The desired baud rate in Hz.
   */
  void begin(std::uint32_t baud_rate) {
    // Initialize UART
    uart_init(get_uart_instance(), baud_rate);

    // Set the GPIO pin muxing
    gpio_set_function(TxPin, GPIO_FUNC_UART);
    gpio_set_function(RxPin, GPIO_FUNC_UART);

    _initialized = true;
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
    return uart_getc(get_uart_instance());
  }

  /**
   * @brief Writes a single byte to the UART transmit buffer.
   *
   *
   * This may block if the transmit buffer is full.
   *
   * @param byte The byte to write.
   * @return The number of bytes written (always 1 if successful, 0 if not
   * initialized).
   */
  size_t write(std::uint8_t byte) {
    if (!_initialized) {
      return 0;
    }
    // uart_putc is blocking, waits for space in FIFO
    uart_putc(get_uart_instance(), byte);
    return 1; // Indicate one byte was written
  }

  /**
   * @brief Writes a single byte to the UART transmit buffer without blocking.
   *
   * Checks if there is space in the TX FIFO before writing. If the FIFO is
   * full, the function returns immediately without writing the byte.
   *
   * @param byte The byte to write.
   * @return true if the byte was written successfully, false if FIFO was full
   * or UART not initialized.
   */
  bool write_nonblocking(std::uint8_t byte) {
    if (!_initialized) {
      return false;
    }
    // Check if TX FIFO has space
    if (!uart_is_writable(get_uart_instance())) {
      return false;
    }
    // Write directly to data register without blocking
    uart_putc_raw(get_uart_instance(), byte);
    return true;
  }

  /**
   * @brief Checks if there is data available to read from the UART.
   * @return true if data is available, false otherwise or if not initialized.
   */
  bool available() {
    if (!_initialized) {
      return false;
    }
    return uart_is_readable(get_uart_instance());
  }

private:
  // Helper to get the uart_inst_t* at runtime
  static uart_inst_t *get_uart_instance() {
    if constexpr (uart_id == detail::UartId::UART0) {
      return uart0;
    } else {
      return uart1;
    }
  }

  bool _initialized;
};

} // namespace musin::hal

#endif // MUSIN_HAL_UART_H

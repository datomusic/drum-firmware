#include "musin/hal/UART.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h" // Required for panic, could use assert

namespace {
// Helper function to determine the UART instance based on pins.
// Returns nullptr if the pin combination is invalid for any UART.
uart_inst_t* get_uart_instance(std::uint32_t tx_pin, std::uint32_t rx_pin) {
    if (((tx_pin == 0) && (rx_pin == 1)) || ((tx_pin == 12) && (rx_pin == 13)) ||
        ((tx_pin == 16) && (rx_pin == 17)) || ((tx_pin == 28) && (rx_pin == 29))) {
        return uart0;
    } else if (((tx_pin == 4) && (rx_pin == 5)) || ((tx_pin == 8) && (rx_pin == 9)) ||
               ((tx_pin == 20) && (rx_pin == 21)) || ((tx_pin == 24) && (rx_pin == 25))) {
        return uart1;
    } else {
        return nullptr; // Invalid pin combination for known UARTs
    }
}
} // anonymous namespace


namespace Musin::HAL {

bool UART::init(std::uint32_t tx_pin, std::uint32_t rx_pin, std::uint32_t baud_rate) {
  _uart_instance = get_uart_instance(tx_pin, rx_pin);

  if (!_uart_instance) {
    // Optionally add logging or error handling here
    // For now, just indicate failure.
    _initialized = false;
    return false;
  }

  // Initialize UART
  uart_init(_uart_instance, baud_rate);

  // Set the GPIO pin muxing
  gpio_set_function(tx_pin, GPIO_FUNC_UART);
  gpio_set_function(rx_pin, GPIO_FUNC_UART);

  // According to the SDK, CTS/RTS require explicit enabling if used,
  // but are typically not needed for basic UART.

  _initialized = true;
  return true;
}

std::uint8_t UART::read() {
  if (!_initialized) {
    // Or perhaps panic("UART not initialized");
    return 0;
  }
  // uart_getc is blocking, waits for character
  return uart_getc(_uart_instance);
}

size_t UART::write(std::uint8_t byte) {
  if (!_initialized) {
    return 0;
  }
  // uart_putc is blocking, waits for space in FIFO
  uart_putc(_uart_instance, byte);
  return 1; // Indicate one byte was written
}

bool UART::available() {
  if (!_initialized) {
    return false;
  }
  return uart_is_readable(_uart_instance);
}

} // namespace Musin::HAL

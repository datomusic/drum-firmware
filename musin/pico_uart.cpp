#include "pico_uart.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"

// TODO: Make UART configurable. Currently defaulting to UART 0.
// TODO: Also make pins configurable
#define UART UART_INSTANCE(0)
#define PIN_TX 1
#define PIN_RX 1

void PicoUART::begin(unsigned long baud_rate) {
  uart_init(UART, baud_rate);
  // Set the GPIO pin mux to the UART - 0 is TX, 1 is RX
  gpio_set_function(PIN_TX, GPIO_FUNC_UART);
  gpio_set_function(PIN_RX, GPIO_FUNC_UART);
}

uint8_t PicoUART::read() {
  return uart_getc(UART);
}

size_t PicoUART::write(uint8_t byte) {
  uart_putc(uart0, byte);
  return 1;
}

bool PicoUART::available(void) {
  return uart_is_readable(UART) && uart_is_writable(UART);
}

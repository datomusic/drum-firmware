#include "pico_uart.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"

// TODO: Make UART configurable. Currently defaulting to UART 0.
#define UART UART_INSTANCE(0)

void PicoUART::begin(unsigned long baud_rate) {
  uart_init(UART, 31250);

  // Set the GPIO pin mux to the UART - 0 is TX, 1 is RX
  gpio_set_function(0, GPIO_FUNC_UART);
  gpio_set_function(1, GPIO_FUNC_UART);
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

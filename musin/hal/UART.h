#ifndef MUSIN_HAL_UART_H
#define MUSIN_HAL_UART_H

#include <cstdint>
#include <cstddef> // For size_t

// Forward declaration to avoid including the full SDK header here
struct uart_inst;
typedef struct uart_inst uart_inst_t;

namespace Musin::HAL {

/**
 * @brief Provides a hardware abstraction layer for UART communication
 *        on the RP2040.
 *
 * This class wraps the Pico SDK UART functions, allowing initialization
 * with specific TX/RX pins and providing a consistent interface.
 */
class UART {
public:
  /**
   * @brief Default constructor. The UART is not initialized.
   */
  UART() : _uart_instance(nullptr), _initialized(false) {}

  /**
   * @brief Initializes the UART peripheral associated with the given pins.
   *
   * Determines the correct UART instance (UART0 or UART1) based on the
   * specified TX and RX pins. Configures the pins and the UART peripheral.
   *
   * @param tx_pin The GPIO pin number for the UART TX line.
   * @param rx_pin The GPIO pin number for the UART RX line.
   * @param baud_rate The desired baud rate in Hz.
   * @return true if initialization was successful (valid pins, etc.), false otherwise.
   */
  bool init(std::uint32_t tx_pin, std::uint32_t rx_pin, std::uint32_t baud_rate);

  /**
   * @brief Reads a single byte from the UART receive buffer.
   *
   * This is a blocking call if no data is available. Check `available()` first
   * for non-blocking reads.
   *
   * @return The byte read from the UART. Returns 0 if not initialized.
   */
  std::uint8_t read();

  /**
   * @brief Writes a single byte to the UART transmit buffer.
   *
   * This may block if the transmit buffer is full.
   *
   * @param byte The byte to write.
   * @return The number of bytes written (always 1 if successful, 0 if not initialized).
   */
  size_t write(std::uint8_t byte);

  /**
   * @brief Checks if there is data available to read from the UART.
   *
   * @return true if data is available, false otherwise or if not initialized.
   */
  bool available();

private:
  uart_inst_t* _uart_instance;
  bool _initialized;
};

} // namespace Musin::HAL

#endif // MUSIN_HAL_UART_H

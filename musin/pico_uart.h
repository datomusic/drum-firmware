#ifndef HARDWARE_SERIAL_H_VBZTXRD8
#define HARDWARE_SERIAL_H_VBZTXRD8

#include <stddef.h>
#include <stdint.h>

struct PicoUART {
  void begin(unsigned long baud_rate);
  uint8_t read(void);
  size_t write(uint8_t byte);
  bool available(void);
};

#endif /* end of include guard: HARDWARE_SERIAL_H_VBZTXRD8 */

#ifndef HARDWARE_SERIAL_H_VBZTXRD8
#define HARDWARE_SERIAL_H_VBZTXRD8

#include <stddef.h>
#include <stdint.h>

struct PicoUART {
  int available(void) {
    return 0;
  }

  int read(void) {
    return 0;
  }

  void begin(unsigned long baud_rate) {
  }

  size_t write(uint8_t byte) {
    return 0;
  }
};

#endif /* end of include guard: HARDWARE_SERIAL_H_VBZTXRD8 */

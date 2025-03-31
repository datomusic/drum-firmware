#ifndef MUSIN_AUDIO_AIC3204_H
#define MUSIN_AUDIO_AIC3204_H

#include <stdint.h>
#include <stdbool.h>

#define AIC3204_I2C_ADDR 0x18
#define AIC3204_SOFT_STEPPING_TIMEOUT_MS 1000
#define AIC3204_AMP_ENABLE_THROUGH_CODEC true

// class Aic3204 {
// public:
//   static bool init(unsigned int sda_pin, unsigned int scl_pin, unsigned int baudrate);
//   static bool write_register(unsigned int page, unsigned int reg_addr, unsigned int value);
//   static bool read_register(unsigned int page, unsigned int reg_addr, unsigned int* read_value);
// };

// C-style interface functions
bool aic3204_init(unsigned int sda_pin, unsigned int scl_pin, unsigned int baudrate);
bool aic3204_write_register(unsigned int page, unsigned int reg_addr, unsigned int value);
bool aic3204_read_register(unsigned int page, unsigned int reg_addr, unsigned int* read_value);

#endif // MUSIN_AUDIO_AIC3204_H

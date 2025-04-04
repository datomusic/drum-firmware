#ifndef MUSIN_AUDIO_AIC3204_H
#define MUSIN_AUDIO_AIC3204_H

#include <stdint.h>
#include <stdbool.h>

#define AIC3204_I2C_ADDR 0x18
#define AIC3204_SOFT_STEPPING_TIMEOUT_MS 1000
#define AIC3204_AMP_ENABLE_THROUGH_CODEC true

#ifdef __cplusplus
extern "C" {
#endif
// C-style interface functions
bool aic3204_init(uint8_t sda_pin, uint8_t scl_pin, uint32_t baudrate);
bool aic3204_write_register(uint8_t page, uint8_t reg_addr, uint8_t value);
bool aic3204_read_register(uint8_t page, uint8_t reg_addr, uint8_t* read_value);
bool aic3204_amp_enable(void);
bool aic3204_amp_disable(void);

#ifdef __cplusplus
} // extern "C"
#endif
#endif // MUSIN_AUDIO_AIC3204_H

/*
 * Custom board definition for DATO Submarine
 */

// pico_cmake_set PICO_PLATFORM=rp2350

#ifndef _BOARDS_DATO_SUBMARINE_H
#define _BOARDS_DATO_SUBMARINE_H

// For board detection
#define DATO_SUBMARINE

// --- UART ---
#ifndef PICO_DEFAULT_UART
#define PICO_DEFAULT_UART 0
#endif
#ifndef PICO_DEFAULT_UART_TX_PIN
#define PICO_DEFAULT_UART_TX_PIN 0
#endif
#ifndef PICO_DEFAULT_UART_RX_PIN
#define PICO_DEFAULT_UART_RX_PIN 1
#endif

// --- I2C ---
#ifndef PICO_DEFAULT_I2C
#define PICO_DEFAULT_I2C 0
#endif
#ifndef PICO_DEFAULT_I2C_SDA_PIN
#define PICO_DEFAULT_I2C_SDA_PIN 18
#endif
#ifndef PICO_DEFAULT_I2C_SCL_PIN
#define PICO_DEFAULT_I2C_SCL_PIN 19
#endif

// --- WS2812 ---
#ifndef PICO_DEFAULT_WS2812_PIN
#define PICO_DEFAULT_WS2812_PIN 16
#endif

// --- Audio ---
#ifndef PICO_AUDIO_I2S_DATA_PIN
#define PICO_AUDIO_I2S_DATA_PIN 22
#endif
#ifndef PICO_AUDIO_I2S_CLOCK_PIN_BASE
#define PICO_AUDIO_I2S_CLOCK_PIN_BASE 23
#endif

// --- FLASH ---
#define PICO_BOOT_STAGE2_CHOOSE_W25Q080 1
#ifndef PICO_FLASH_SPI_CLKDIV
#define PICO_FLASH_SPI_CLKDIV 2
#endif
#ifndef PICO_FLASH_SIZE_BYTES
#define PICO_FLASH_SIZE_BYTES (16 * 1024 * 1024)
#endif

#endif

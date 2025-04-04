// --- aic3204.c ---
#include "aic3204.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h> // For printf debugging
#include "pico/time.h" // For sleep_ms, get_absolute_time, time_reached
#include "hardware/gpio.h" // For pin validation
#include "hardware/i2c.h"

// --- Static Variables ---
static i2c_inst_t* _i2c_inst = NULL; // Pointer to the I2C instance used
static uint8_t _current_page = 0xFF; // Cache the current page, 0xFF indicates unknown/uninitialized

// --- Internal Helper Functions ---

/**
 * @brief Checks if a device exists at the specified I2C address.
 */
static bool _aic3204_device_present(i2c_inst_t *i2c, uint8_t addr) {
    if (!i2c) return false;
    uint8_t rxdata;
    // Use a short timeout to avoid blocking forever if the bus is stuck
    int ret = i2c_read_timeout_us(i2c, addr, &rxdata, 1, false, 5000); // 5ms timeout
    return (ret == 1); // Success if 1 byte read
}

/**
 * @brief Low-level I2C write function for the AIC3204.
 */
static bool _aic3204_i2c_write(uint8_t reg_addr, uint8_t value) {
    if (!_i2c_inst) return false;
    uint8_t data[2] = {reg_addr, value};
    // Use timeout version for robustness
    int ret = i2c_write_timeout_us(_i2c_inst, AIC3204_I2C_ADDR, data, 2, true, 10000); // 10ms timeout
    if (ret != 2) {
        // printf("AIC3204 Error: I2C write failed for reg 0x%02X (ret %d)\n", reg_addr, ret);
        return false;
    }
    return true;
}

/**
 * @brief Low-level I2C read function for the AIC3204.
 */
static bool _aic3204_i2c_read(uint8_t reg_addr, uint8_t *value) {
    if (!_i2c_inst || !value) return false;
    // Write the register address we want to read from (with NO_STOP)
    int ret_w = i2c_write_timeout_us(_i2c_inst, AIC3204_I2C_ADDR, &reg_addr, 1, true, 5000); // 5ms timeout, send STOP after address
    if (ret_w != 1) {
        // printf("AIC3204 Error: I2C read failed (write addr phase) for reg 0x%02X (ret %d)\n", reg_addr, ret_w);
        return false;
    }
    // Read the data byte
    int ret_r = i2c_read_timeout_us(_i2c_inst, AIC3204_I2C_ADDR, value, 1, false, 5000); // 5ms timeout
    if (ret_r != 1) {
        // printf("AIC3204 Error: I2C read failed (read data phase) for reg 0x%02X (ret %d)\n", reg_addr, ret_r);
        return false;
    }
    return true;
}


/**
 * @brief Determines the I2C instance based on SDA/SCL pins.
 */
static i2c_inst_t* _get_i2c_instance(uint sda_pin, uint scl_pin) {
    // Check i2c0 pins
    bool sda_is_i2c0 = (sda_pin % 4 == 0 && sda_pin <= 20);
    bool scl_is_i2c0 = (scl_pin % 4 == 1 && scl_pin <= 21);
    if (sda_is_i2c0 && scl_is_i2c0) return i2c0;

    // Check i2c1 pins
    bool sda_is_i2c1 = ((sda_pin % 4 == 2 && sda_pin <= 18) || sda_pin == 26);
    bool scl_is_i2c1 = ((scl_pin % 4 == 3 && scl_pin <= 19) || scl_pin == 27);
    if (sda_is_i2c1 && scl_is_i2c1) return i2c1;

    return NULL; // Invalid pin combination
}

/**
 * @brief Selects the target page if it's not the current one. Internal use.
 */
static bool _aic3204_select_page(uint8_t page) {
    if (!_i2c_inst) return false; // Must be initialized

    if (page != _current_page) {
        if (!_aic3204_i2c_write(0x00, page)) {
            printf("AIC3204 Error: Failed to select page %d\n", page);
            _current_page = 0xFF; // Mark page as unknown on error
            return false;
        }
        _current_page = page;
        // Datasheet doesn't specify delay, but a tiny one might not hurt
        // sleep_us(50);
    }
    return true;
}

// --- Public API Functions ---

bool aic3204_write_register(uint8_t page, uint8_t reg_addr, uint8_t value) {
    if (!_i2c_inst) {
         printf("AIC3204 Error: Attempted write before successful initialization.\n");
         return false;
    }

    // Select the page (handles _current_page update)
    // Skip page select if writing to Page Select Register itself
    if (!(page == 0 && reg_addr == 0)) {
        if (!_aic3204_select_page(page)) {
            return false; // Page selection failed
        }
    } else {
        // If writing to page select register, update cache *after* write
    }

    // Write the actual register data
    if (!_aic3204_i2c_write(reg_addr, value)) {
        printf("AIC3204 Error: Failed writing value 0x%02X to Page %d, Reg 0x%02X\n", value, _current_page, reg_addr);
        return false;
    }

    // Update cached page if we just wrote to the page select register
    if (page == 0 && reg_addr == 0) {
         _current_page = value;
    }

    return true;
}

bool aic3204_read_register(uint8_t page, uint8_t reg_addr, uint8_t *read_value) {
     if (!_i2c_inst) {
         printf("AIC3204 Error: Attempted read before successful initialization.\n");
         return false;
     }
     if (!read_value) {
         printf("AIC3204 Error: NULL pointer provided for read_value.\n");
         return false;
     }

    // Select the page (handles _current_page update)
    // Cannot read page select register 0x00,0x00 itself meaningfully this way
    if (page == 0 && reg_addr == 0) {
        printf("AIC3204 Warning: Reading Page 0, Reg 0 (Page Select) might not be meaningful.\n");
        // We can return the cached value, but it's better to just read another register on the target page.
        // *read_value = _current_page;
        // return true;
        // Let's proceed with the read, it might return something, likely 0xFF if nothing written recently.
    }

    if (!_aic3204_select_page(page)) {
        return false; // Page selection failed
    }

    // Read the register value
    if (!_aic3204_i2c_read(reg_addr, read_value)) {
        printf("AIC3204 Error: Failed reading from Page %d, Reg 0x%02X\n", _current_page, reg_addr);
        return false;
    }

    return true;
}


bool aic3204_init(uint8_t sda_pin, uint8_t scl_pin, uint32_t baudrate) {
    printf("Initializing AIC3204 on SDA=GP%u, SCL=GP%u...\n", sda_pin, scl_pin);

    // Determine I2C instance based on pins
    _i2c_inst = _get_i2c_instance(sda_pin, scl_pin);

    if (!_i2c_inst) {
        printf("AIC3204 Error: Invalid I2C pin combination (SDA=GP%u, SCL=GP%u).\n", sda_pin, scl_pin);
        printf("Valid pairs: i2c0 (SDA:0,4,8,12,16,20 | SCL:1,5,9,13,17,21), i2c1 (SDA:2,6,10,14,18,26 | SCL:3,7,11,15,19,27)\n");
        return false;
    }
    printf("Using I2C instance: %s\n", (_i2c_inst == i2c0) ? "i2c0" : "i2c1");

    // Initialize I2C
    uint actual_baudrate = i2c_init(_i2c_inst, baudrate);
    printf("I2C Initialized at %u Hz\n", actual_baudrate);

    // Setup GPIO pins
    gpio_set_function(sda_pin, GPIO_FUNC_I2C);
    gpio_set_function(scl_pin, GPIO_FUNC_I2C);
    gpio_pull_up(sda_pin);
    gpio_pull_up(scl_pin);

    // Give I2C bus/pins time to settle
    sleep_ms(10);

    // Scan for the device
    printf("Scanning for AIC3204 at address 0x%02X...\n", AIC3204_I2C_ADDR);
    if (!_aic3204_device_present(_i2c_inst, AIC3204_I2C_ADDR)) {
        printf("AIC3204 Error: Device not found at address 0x%02X\n", AIC3204_I2C_ADDR);
        i2c_deinit(_i2c_inst);
        gpio_set_function(sda_pin, GPIO_FUNC_NULL);
        gpio_set_function(scl_pin, GPIO_FUNC_NULL);
        gpio_disable_pulls(sda_pin);
        gpio_disable_pulls(scl_pin);
        _i2c_inst = NULL;
        return false;
    }
    printf("AIC3204 Found!\n");

    // --- Start Initialization Sequence ---
    printf("Initializing AIC3204 codec registers...\n");
    bool success = true;
    _current_page = 0xFF; // Force page selection on first write

    // Initialize to Page 0 & Software reset
    success &= aic3204_write_register(0x00, 0x00, 0x00);
    success &= aic3204_write_register(0x00, 0x01, 0x01);
    sleep_ms(5); // Wait for reset

    // Disable the external amp initially
    success &= aic3204_write_register(0x00, 0x37, 0x00); // MFP4 as GPIO Output, LOW

    // PLL and Clock Configuration (Page 0)
    success &= aic3204_write_register(0x00, 0x04, 0x07);
    success &= aic3204_write_register(0x00, 0x05, 0x93); // PLL ON, P=1, R=3
    success &= aic3204_write_register(0x00, 0x06, 0x14); // J=20
    success &= aic3204_write_register(0x00, 0x07, 0x00); // D=0 MSB
    success &= aic3204_write_register(0x00, 0x08, 0x00); // D=0 LSB
    success &= aic3204_write_register(0x00, 0x0B, 0x85); // NDAC = 5, ON
    success &= aic3204_write_register(0x00, 0x0C, 0x83); // MDAC = 3, ON
    success &= aic3204_write_register(0x00, 0x0D, 0x00); // DOSR = 128 MSB
    success &= aic3204_write_register(0x00, 0x0E, 0x80); // DOSR = 128 LSB

    // Audio Interface Settings (Page 0)
    success &= aic3204_write_register(0x00, 0x1B, 0x00); // I2S, 16 bit
    success &= aic3204_write_register(0x00, 0x19, 0x00); // BCLK/WCLK inputs

    // DAC Processing Block (Page 0)
    success &= aic3204_write_register(0x00, 0x3C, 0x08); // DAC PRB_P8

    // --- Power and Analog Configuration (Page 1) ---
    // No need to write page 1 select here, write_register handles it
    // success &= aic3204_write_register(0x00, 0x00, 0x01);

    // Power Settings
    success &= aic3204_write_register(0x01, 0x01, 0x08); // Disable Crude AVdd
    success &= aic3204_write_register(0x01, 0x02, 0x00); // Analog Blocks OFF
    success &= aic3204_write_register(0x01, 0x02, 0x01); // Master Analog ON, AVDD LDO ON
    success &= aic3204_write_register(0x01, 0x0A, 0x33); // HP CM=1.65V, Lineout CM=0.9V, LDO=1.72V

    // DAC/ADC PTM modes (Page 1)
    success &= aic3204_write_register(0x01, 0x03, 0x00); // DAC PTM = P3/4
    success &= aic3204_write_register(0x01, 0x04, 0x00); // ADC PTM = R4

    // Power-up Timing (Page 1)
    success &= aic3204_write_register(0x01, 0x47, 0x32); // Input power-up time 3.1ms
    success &= aic3204_write_register(0x01, 0x7B, 0x01); // REF charging time 40ms

    // --- Output Driver Configuration (Page 1) ---
    // Headphone Routing & Gain (0dB)
    success &= aic3204_write_register(0x01, 0x14, 0x05); // Slowly ramp up HP drivers
    success &= aic3204_write_register(0x01, 0x0C, 0x08); // DAC_L -> HPL
    success &= aic3204_write_register(0x01, 0x0D, 0x08); // DAC_R -> HPR
    success &= aic3204_write_register(0x01, 0x10, 0x00); // HPL Gain 0dB
    success &= aic3204_write_register(0x01, 0x11, 0x00); // HPR Gain 0dB

    // Line Output Routing & Gain (Python's differential config, 0dB Gain)
    success &= aic3204_write_register(0x01, 0x0E, 0x01); // LOL Diff Config
    success &= aic3204_write_register(0x01, 0x0F, 0x08); // LOR Diff Config
    success &= aic3204_write_register(0x01, 0x12, 0x00); // LOL Gain 0dB
    success &= aic3204_write_register(0x01, 0x13, 0x00); // LOR Gain 0dB

    // Power up Output Drivers (Page 1) - This starts soft-stepping
    success &= aic3204_write_register(0x01, 0x09, 0x3C); // Power up HPL, HPR, LOL, LOR

    // --- Wait for soft-stepping completion ---
    if (success) {
        printf("Waiting for codec soft-stepping completion (max %d ms)...\n", AIC3204_SOFT_STEPPING_TIMEOUT_MS);
        absolute_time_t start_time = get_absolute_time();
        absolute_time_t timeout_time = make_timeout_time_ms(AIC3204_SOFT_STEPPING_TIMEOUT_MS);
        bool soft_stepping_complete = false;
        uint8_t status_reg = 0;
        const uint8_t SOFT_STEPPING_REG = 0x3F; // Reg 63 decimal

        while (!time_reached(timeout_time)) {
            // Read Page 1, Register 63 (0x3F)
            if (aic3204_read_register(1, SOFT_STEPPING_REG, &status_reg)) {
                // Check bits 7:6 (mask 0xC0)
                if ((status_reg & 0xC0) == 0xC0) {
                    soft_stepping_complete = true;
                    absolute_time_t end_time = get_absolute_time();
                    int64_t elapsed_us = absolute_time_diff_us(start_time, end_time);
                    printf("Soft-stepping complete in %lld ms (Reg 0x%02X = 0x%02X).\n", elapsed_us / 1000, SOFT_STEPPING_REG, status_reg);
                    break; // Exit loop
                }
            } else {
                // Read failed, maybe retry or abort? Let's retry after a delay.
                printf("Warning: Failed to read soft-stepping status register.\n");
            }
            // Wait a bit before polling again
            sleep_ms(10);
        }

        if (!soft_stepping_complete) {
            printf("AIC3204 Warning: Timeout waiting for soft-stepping completion.\n");
            // Do not mark initialization as failed. There might be a slight pop
        }
    }

    // --- Final DAC Setup (Page 0) ---
    if (success) {
        success &= aic3204_write_register(0x00, 0x00, 0x00); // Select Page 0
        success &= aic3204_write_register(0x00, 0x3F, 0xD6); // Power up L&R DAC Channels (Digital)
        success &= aic3204_write_register(0x00, 0x40, 0x00); // Unmute DAC digital volume, 0dB gain
    }

    // --- Final Check and Cleanup ---
    if (!success) {
        printf("AIC3204 Error: Initialization failed at some step.\n");
        if (_i2c_inst) { // Check if I2C was even initialized
            i2c_deinit(_i2c_inst);
            gpio_set_function(sda_pin, GPIO_FUNC_NULL);
            gpio_set_function(scl_pin, GPIO_FUNC_NULL);
            gpio_disable_pulls(sda_pin);
            gpio_disable_pulls(scl_pin);
        }
        _i2c_inst = NULL;
        _current_page = 0xFF;
        return false;
    }

    printf("AIC3204 register initialization complete.\n");

    aic3204_amp_set_enabled(true);
    
    return true; // Initialization successful
}

bool aic3204_amp_set_enabled(bool enable) {
#if AIC3204_AMP_ENABLE_THROUGH_CODEC == 1
    printf("%s external AMP via Codec GPIO MFP4 (%s)...\n", 
           enable ? "Enabling" : "Disabling", 
           enable ? "HIGH" : "LOW");
    uint8_t value = enable ? 0x05 : 0x00;
    bool amp_success = aic3204_write_register(0x00, 0x37, value);
    if (!amp_success) {
        printf("AIC3204 Warning: Failed to set MFP4 %s to %s amp.\n", 
               enable ? "HIGH" : "LOW", 
               enable ? "enable" : "disable");
    }
    sleep_ms(10); // Small delay after changing amp state
    return amp_success;
#endif
    printf("AIC3204 Warning: External AMP is not managed through the codec.");
    return false;
}

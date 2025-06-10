#include "aic3204.hpp"

#include <climits> // For INT8_MIN
#include <cstdio>  // For printf

// Wrap C SDK headers in extern "C"
extern "C" {
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pico/time.h"
}

namespace musin::drivers {

// --- Constructor / Destructor ---

Aic3204::Aic3204(uint8_t sda_pin, uint8_t scl_pin, uint32_t baudrate)
    : _sda_pin(sda_pin), _scl_pin(scl_pin) {
  printf("Initializing AIC3204 on SDA=GP%u, SCL=GP%u...\n", _sda_pin, _scl_pin);

  _i2c_inst = get_i2c_instance(_sda_pin, _scl_pin);
  if (!_i2c_inst) {
    printf("AIC3204 Error: Invalid I2C pin combination (SDA=GP%u, SCL=GP%u).\n",
           _sda_pin, _scl_pin);
    printf("Valid pairs: i2c0 (SDA:0,4,8,12,16,20 | SCL:1,5,9,13,17,21), i2c1 "
           "(SDA:2,6,10,14,18,26 | SCL:3,7,11,15,19,27)\n");
    return; // _is_initialized remains false
  }
  printf("Using I2C instance: %s\n", (_i2c_inst == i2c0) ? "i2c0" : "i2c1");

  uint actual_baudrate = i2c_init(_i2c_inst, baudrate);
  printf("I2C Initialized at %u Hz\n", actual_baudrate);

  gpio_set_function(_sda_pin, GPIO_FUNC_I2C);
  gpio_set_function(_scl_pin, GPIO_FUNC_I2C);
  gpio_pull_up(_sda_pin);
  gpio_pull_up(_scl_pin);

  sleep_ms(10);

  printf("Scanning for AIC3204 at address 0x%02X...\n", I2C_ADDR);
  if (!device_present(I2C_ADDR)) {
    printf("AIC3204 Error: Device not found at address 0x%02X\n", I2C_ADDR);
    return; // Destructor will handle cleanup via RAII
  }
  printf("AIC3204 Found!\n");

  printf("Initializing AIC3204 codec registers...\n");
  _current_page = 0xFF; // Force page selection on first write

  // --- Start Initialization Sequence (Fail-Fast) ---
  // Initialize to Page 0 & Software reset
  if (write_register(0x00, 0x00, 0x00) != Aic3204Status::OK) return;
  if (write_register(0x00, 0x01, 0x01) != Aic3204Status::OK) return;
  sleep_ms(5); // Wait for reset

  // Disable the external amp initially
  if (write_register(0x00, 0x37, 0x00) != Aic3204Status::OK) return; // MFP4 as GPIO Output, LOW

  // PLL and Clock Configuration (Page 0)
  if (write_register(0x00, 0x04, 0x07) != Aic3204Status::OK) return;
  if (write_register(0x00, 0x05, 0x93) != Aic3204Status::OK) return; // PLL ON, P=1, R=3
  if (write_register(0x00, 0x06, 0x14) != Aic3204Status::OK) return; // J=20
  if (write_register(0x00, 0x07, 0x00) != Aic3204Status::OK) return; // D=0 MSB
  if (write_register(0x00, 0x08, 0x00) != Aic3204Status::OK) return; // D=0 LSB
  if (write_register(0x00, 0x0B, 0x85) != Aic3204Status::OK) return; // NDAC = 5, ON
  if (write_register(0x00, 0x0C, 0x83) != Aic3204Status::OK) return; // MDAC = 3, ON
  if (write_register(0x00, 0x0D, 0x00) != Aic3204Status::OK) return; // DOSR = 128 MSB
  if (write_register(0x00, 0x0E, 0x80) != Aic3204Status::OK) return; // DOSR = 128 LSB

  // Audio Interface Settings (Page 0)
  if (write_register(0x00, 0x1B, 0x00) != Aic3204Status::OK) return; // I2S, 16 bit
  if (write_register(0x00, 0x19, 0x00) != Aic3204Status::OK) return; // BCLK/WCLK inputs

  // DAC Processing Block (Page 0)
  if (write_register(0x00, 0x3C, 0x08) != Aic3204Status::OK) return; // DAC PRB_P8
  // --- Power and Analog Configuration (Page 1) ---
  // Power Settings
  if (write_register(0x01, 0x01, 0x08) != Aic3204Status::OK) return; // Disable Crude AVdd
  if (write_register(0x01, 0x02, 0x00) != Aic3204Status::OK) return; // Analog Blocks OFF
  if (write_register(0x01, 0x02, 0x01) != Aic3204Status::OK) return; // Master Analog ON, AVDD LDO ON
  if (write_register(0x01, 0x0A, 0x33) != Aic3204Status::OK) return; // HP CM=1.65V, Lineout CM=0.9V, LDO=1.72V

  // DAC/ADC PTM modes (Page 1)
  if (write_register(0x01, 0x03, 0x00) != Aic3204Status::OK) return; // DAC PTM = P3/4
  if (write_register(0x01, 0x04, 0x00) != Aic3204Status::OK) return; // ADC PTM = R4

  // Power-up Timing (Page 1)
  if (write_register(0x01, 0x47, 0x32) != Aic3204Status::OK) return; // Input power-up time 3.1ms
  if (write_register(0x01, 0x7B, 0x01) != Aic3204Status::OK) return; // REF charging time 40ms

  // --- Output Driver Configuration (Page 1) ---
  // Headphone Routing & Gain (0dB)
  if (write_register(0x01, 0x14, 0x05) != Aic3204Status::OK) return; // Slowly ramp up HP drivers
  if (write_register(0x01, 0x0C, 0x08) != Aic3204Status::OK) return; // DAC_L -> HPL
  if (write_register(0x01, 0x0D, 0x08) != Aic3204Status::OK) return; // DAC_R -> HPR
  if (write_register(0x01, 0x10, 0x00) != Aic3204Status::OK) return; // HPL Gain 0dB
  if (write_register(0x01, 0x11, 0x00) != Aic3204Status::OK) return; // HPR Gain 0dB

  // Line Output Routing & Gain (Python's differential config, 0dB Gain)
  if (write_register(0x01, 0x0E, 0x01) != Aic3204Status::OK) return; // LOL Diff Config
  if (write_register(0x01, 0x0F, 0x08) != Aic3204Status::OK) return; // LOR Diff Config
  if (write_register(0x01, 0x12, 0x00) != Aic3204Status::OK) return; // LOL Gain 0dB
  if (write_register(0x01, 0x13, 0x00) != Aic3204Status::OK) return; // LOR Gain 0dB

  // Power up Output Drivers (Page 1) - This starts soft-stepping
  if (write_register(0x01, 0x09, 0x3C) != Aic3204Status::OK) return; // Power up HPL, HPR, LOL, LOR

  // --- Wait for soft-stepping completion ---
  if (wait_for_soft_stepping() == Aic3204Status::ERROR_STEPPING_TIMEOUT) {
    printf("AIC3204 Warning: Timeout waiting for soft-stepping completion.\n");
  }

  // --- Final DAC Setup (Page 0) ---
  if (write_register(0x00, 0x00, 0x00) != Aic3204Status::OK) return; // Select Page 0
  if (write_register(0x00, 0x3F, 0xD6) != Aic3204Status::OK) return; // Power up L&R DAC Channels (Digital)
  if (write_register(0x00, 0x40, 0x00) != Aic3204Status::OK) return; // Unmute DAC digital volume, 0dB gain

  _is_initialized = true;
  printf("AIC3204 register initialization complete.\n");

  set_amp_enabled(true);
}

Aic3204::~Aic3204() {
  if (_i2c_inst) {
    if (_is_initialized) {
      set_amp_enabled(false);
    }
    i2c_deinit(_i2c_inst);
    gpio_set_function(_sda_pin, GPIO_FUNC_NULL);
    gpio_set_function(_scl_pin, GPIO_FUNC_NULL);
    gpio_disable_pulls(_sda_pin);
    gpio_disable_pulls(_scl_pin);
    printf("AIC3204 De-initialized.\n");
  }
}

bool Aic3204::is_initialized() const { return _is_initialized; }

// --- Public API Methods ---

Aic3204Status Aic3204::write_register(uint8_t page, uint8_t reg_addr,
                                      uint8_t value) {
  if (!_i2c_inst) {
    return Aic3204Status::ERROR_NOT_INITIALIZED;
  }

  if (!(page == 0 && reg_addr == 0)) {
    Aic3204Status status = select_page(page);
    if (status != Aic3204Status::OK) {
      return status;
    }
  }

  Aic3204Status status = i2c_write(reg_addr, value);
  if (status != Aic3204Status::OK) {
    printf("AIC3204 Error: Failed writing value 0x%02X to Page %d, Reg 0x%02X\n",
           value, _current_page, reg_addr);
    return status;
  }

  if (page == 0 && reg_addr == 0) {
    _current_page = value;
  }

  return Aic3204Status::OK;
}

Aic3204Status Aic3204::read_register(uint8_t page, uint8_t reg_addr,
                                     uint8_t &read_value) {
  if (!_i2c_inst) {
    return Aic3204Status::ERROR_NOT_INITIALIZED;
  }

  if (page == 0 && reg_addr == 0) {
    printf("AIC3204 Warning: Reading Page 0, Reg 0 (Page Select) might not be "
           "meaningful.\n");
  }

  Aic3204Status status = select_page(page);
  if (status != Aic3204Status::OK) {
    return status;
  }

  status = i2c_read(reg_addr, read_value);
  if (status != Aic3204Status::OK) {
    printf("AIC3204 Error: Failed reading from Page %d, Reg 0x%02X\n",
           _current_page, reg_addr);
    return status;
  }

  return Aic3204Status::OK;
}

Aic3204Status Aic3204::set_amp_enabled(bool enable) {
  if (!is_initialized())
    return Aic3204Status::ERROR_NOT_INITIALIZED;

  if (AMP_ENABLE_THROUGH_CODEC) {
    printf("%s external AMP via Codec GPIO MFP4 (%s)...\n",
           enable ? "Enabling" : "Disabling", enable ? "HIGH" : "LOW");
    uint8_t value = enable ? 0x05 : 0x00;
    Aic3204Status status = write_register(0x00, 0x37, value);
    if (status != Aic3204Status::OK) {
      printf("AIC3204 Warning: Failed to set MFP4 %s to %s amp.\n",
             enable ? "HIGH" : "LOW", enable ? "enable" : "disable");
    }
    sleep_ms(10);
    return status;
  }
  printf("AIC3204 Warning: External AMP is not managed through the codec.");
  return Aic3204Status::OK;
}

Aic3204Status Aic3204::set_dac_volume(int8_t volume) {
  if (!is_initialized())
    return Aic3204Status::ERROR_NOT_INITIALIZED;

  if (volume < -127 || volume > 48) {
    printf("AIC3204 Error: DAC volume %d invalid. Valid range: -127 to +48\n",
           volume);
    return Aic3204Status::ERROR_INVALID_ARG;
  }

  if (volume == _current_dac_volume) {
    return Aic3204Status::OK;
  }

  if (is_soft_stepping()) {
    printf(
        "AIC3204 Warning: Cannot set DAC volume while soft-stepping is active.\n");
    return Aic3204Status::ERROR_STEPPING_ACTIVE;
  }

  uint8_t reg_value = static_cast<uint8_t>(volume);
  Aic3204Status status_l = write_register(0x00, 0x41, reg_value);
  Aic3204Status status_r = write_register(0x00, 0x42, reg_value);

  if (status_l == Aic3204Status::OK && status_r == Aic3204Status::OK) {
    printf("AIC3204: DAC volume set to %+d (%.1fdB)\n", volume, volume * 0.5f);
    _current_dac_volume = volume;
    return Aic3204Status::OK;
  } else {
    printf("AIC3204 Error: Failed to write DAC volume registers\n");
    _current_dac_volume = INT8_MIN; // Invalidate cache
    return (status_l != Aic3204Status::OK) ? status_l : status_r;
  }
}

Aic3204Status Aic3204::route_in_to_headphone(bool enable) {
  if (!is_initialized())
    return Aic3204Status::ERROR_NOT_INITIALIZED;

  const uint8_t PAGE = 1;
  const uint8_t HPL_REG = 0x0C;
  const uint8_t HPR_REG = 0x0D;
  const uint8_t IN1_TO_HP_MASK = (1 << 2);

  printf("AIC3204: %s routing IN1 to Headphone Output.\n",
         enable ? "Enabling" : "Disabling");

  uint8_t hpl_val = 0;
  Aic3204Status status = read_register(PAGE, HPL_REG, hpl_val);
  if (status != Aic3204Status::OK) {
    return status;
  }

  uint8_t new_hpl_val =
      enable ? (hpl_val | IN1_TO_HP_MASK) : (hpl_val & ~IN1_TO_HP_MASK);
  if (new_hpl_val != hpl_val) {
    status = write_register(PAGE, HPL_REG, new_hpl_val);
    if (status != Aic3204Status::OK) {
      return status;
    }
  }

  uint8_t hpr_val = 0;
  status = read_register(PAGE, HPR_REG, hpr_val);
  if (status != Aic3204Status::OK) {
    return status;
  }

  uint8_t new_hpr_val =
      enable ? (hpr_val | IN1_TO_HP_MASK) : (hpr_val & ~IN1_TO_HP_MASK);
  if (new_hpr_val != hpr_val) {
    status = write_register(PAGE, HPR_REG, new_hpr_val);
    if (status != Aic3204Status::OK) {
      return status;
    }
  }

  return Aic3204Status::OK;
}

// --- Private Helper Methods ---

i2c_inst_t *Aic3204::get_i2c_instance(uint8_t sda_pin, uint8_t scl_pin) {
  bool sda_is_i2c0 = (sda_pin % 4 == 0 && sda_pin <= 20);
  bool scl_is_i2c0 = (scl_pin % 4 == 1 && scl_pin <= 21);
  if (sda_is_i2c0 && scl_is_i2c0)
    return i2c0;

  bool sda_is_i2c1 = ((sda_pin % 4 == 2 && sda_pin <= 18) || sda_pin == 26);
  bool scl_is_i2c1 = ((scl_pin % 4 == 3 && scl_pin <= 19) || scl_pin == 27);
  if (sda_is_i2c1 && scl_is_i2c1)
    return i2c1;

  return nullptr;
}

bool Aic3204::device_present(uint8_t addr) {
  if (!_i2c_inst)
    return false;
  uint8_t rxdata;
  int ret = i2c_read_timeout_us(_i2c_inst, addr, &rxdata, 1, false, 5000);
  return (ret == 1);
}

Aic3204Status Aic3204::i2c_write(uint8_t reg_addr, uint8_t value) {
  if (!_i2c_inst)
    return Aic3204Status::ERROR_NOT_INITIALIZED;
  uint8_t data[2] = {reg_addr, value};
  int ret = i2c_write_timeout_us(_i2c_inst, I2C_ADDR, data, 2, true, 10000);
  if (ret != 2) {
    return Aic3204Status::ERROR_I2C_WRITE_FAILED;
  }
  return Aic3204Status::OK;
}

Aic3204Status Aic3204::i2c_read(uint8_t reg_addr, uint8_t &value) {
  if (!_i2c_inst)
    return Aic3204Status::ERROR_NOT_INITIALIZED;
  int ret_w =
      i2c_write_timeout_us(_i2c_inst, I2C_ADDR, &reg_addr, 1, true, 5000);
  if (ret_w != 1) {
    return Aic3204Status::ERROR_I2C_WRITE_FAILED;
  }
  int ret_r = i2c_read_timeout_us(_i2c_inst, I2C_ADDR, &value, 1, false, 5000);
  if (ret_r != 1) {
    return Aic3204Status::ERROR_I2C_READ_FAILED;
  }
  return Aic3204Status::OK;
}

Aic3204Status Aic3204::select_page(uint8_t page) {
  if (!_i2c_inst)
    return Aic3204Status::ERROR_NOT_INITIALIZED;

  if (page != _current_page) {
    if (i2c_write(0x00, page) != Aic3204Status::OK) {
      printf("AIC3204 Error: Failed to select page %d\n", page);
      _current_page = 0xFF; // Mark page as unknown on error
      return Aic3204Status::ERROR_I2C_WRITE_FAILED;
    }
    _current_page = page;
  }
  return Aic3204Status::OK;
}

bool Aic3204::is_soft_stepping() {
  uint8_t status_reg = 0;
  const uint8_t SOFT_STEPPING_PAGE = 1;
  const uint8_t SOFT_STEPPING_REG = 0x3F;
  const uint8_t SOFT_STEPPING_COMPLETE_MASK = 0xC0;

  if (read_register(SOFT_STEPPING_PAGE, SOFT_STEPPING_REG, status_reg) ==
      Aic3204Status::OK) {
    return (status_reg & SOFT_STEPPING_COMPLETE_MASK) !=
           SOFT_STEPPING_COMPLETE_MASK;
  } else {
    printf("AIC3204 Warning: Failed to read soft-stepping status register. "
           "Assuming active.\n");
    return true; // Fail-safe
  }
}

Aic3204Status Aic3204::wait_for_soft_stepping() {
  printf("Waiting for codec soft-stepping completion (max %d ms)...\n",
         SOFT_STEPPING_TIMEOUT_MS);
  absolute_time_t timeout_time =
      make_timeout_time_ms(SOFT_STEPPING_TIMEOUT_MS);

  while (!time_reached(timeout_time)) {
    if (!is_soft_stepping()) {
      printf("Soft-stepping complete.\n");
      return Aic3204Status::OK;
    }
    sleep_ms(10);
  }

  return Aic3204Status::ERROR_STEPPING_TIMEOUT;
}

} // namespace musin::drivers

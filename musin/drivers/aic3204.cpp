#include "aic3204.hpp"

#include <climits> // For INT8_MIN
#include <cstdio>  // For printf

// --- Logging Configuration ---
#define AIC3204_ENABLE_LOGGING 0 // Set to 0 to disable all logging

#if AIC3204_ENABLE_LOGGING
#define AIC_LOG(format, ...) printf("AIC3204: " format "\n", ##__VA_ARGS__)
#else
#define AIC_LOG(format, ...) ((void)0)
#endif

// Wrap C SDK headers in extern "C"
extern "C" {
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pico/time.h"
}

namespace musin::drivers {

// --- Constructor / Destructor ---

Aic3204::Aic3204(uint8_t sda_pin, uint8_t scl_pin, uint32_t baudrate,
                 uint8_t reset_pin)
    : _sda_pin(sda_pin), _scl_pin(scl_pin), _reset_pin(reset_pin) {
  if (_reset_pin != 0xFF) {
    AIC_LOG("Initializing AIC3204 on SDA=GP%u, SCL=GP%u, RST=GP%u...", _sda_pin,
            _scl_pin, _reset_pin);
    gpio_init(_reset_pin);
    gpio_set_dir(_reset_pin, GPIO_OUT);
    gpio_put(_reset_pin, 0); // Set LOW
  } else {
    AIC_LOG("Initializing AIC3204 on SDA=GP%u, SCL=GP%u...", _sda_pin,
            _scl_pin);
  }

  _i2c_inst = get_i2c_instance(_sda_pin, _scl_pin);
  if (!_i2c_inst) {
    AIC_LOG("AIC3204 Error: Invalid I2C pin combination (SDA=GP%u, SCL=GP%u).",
            _sda_pin, _scl_pin);
    AIC_LOG("Valid pairs: i2c0 (SDA:0,4,8,12,16,20 | SCL:1,5,9,13,17,21), i2c1 "
            "(SDA:2,6,10,14,18,26 | SCL:3,7,11,15,19,27)");
    return; // _is_initialized remains false
  }
  AIC_LOG("Using I2C instance: %s", (_i2c_inst == i2c0) ? "i2c0" : "i2c1");

  [[maybe_unused]] uint actual_baudrate = i2c_init(_i2c_inst, baudrate);
  AIC_LOG("I2C Initialized at %u Hz", actual_baudrate);

  gpio_set_function(_sda_pin, GPIO_FUNC_I2C);
  gpio_set_function(_scl_pin, GPIO_FUNC_I2C);
  gpio_pull_up(_sda_pin);
  gpio_pull_up(_scl_pin);

  sleep_ms(1);

  if (_reset_pin != 0xFF) {
    gpio_put(_reset_pin, 1); // Set HIGH
    sleep_ms(1);             // Give codec time to wake up after reset
  }

  AIC_LOG("Scanning for AIC3204 at address 0x%02X...", I2C_ADDR);
  if (!device_present(I2C_ADDR)) {
    AIC_LOG("AIC3204 Error: Device not found at address 0x%02X", I2C_ADDR);
    return; // Destructor will handle cleanup via RAII
  }
  AIC_LOG("AIC3204 Found!");

  AIC_LOG("Initializing AIC3204 codec registers...");
  _current_page = 0xFF; // Force page selection on first write

  // --- Start Initialization Sequence (Fail-Fast) ---
  // Initialize to Page 0 & Software reset
  if (write_register(0x00, 0x00, 0x00) != Aic3204Status::OK) {
    return;
  }
  if (write_register(0x00, 0x01, 0x01) != Aic3204Status::OK) {
    return;
  }
  sleep_ms(5); // Wait for reset

  // Disable the external amp initially
  if (write_register(0x00, 0x37, 0x00) != Aic3204Status::OK) {
    return; // MFP4 as GPIO Output, LOW
  }

  // PLL and Clock Configuration (Page 0)
  if (write_register(0x00, 0x04, 0x07) != Aic3204Status::OK) {
    return;
  }
  if (write_register(0x00, 0x05, 0x93) != Aic3204Status::OK) {
    return; // PLL ON, P=1, R=3
  }
  if (write_register(0x00, 0x06, 0x14) != Aic3204Status::OK) {
    return; // J=20
  }
  if (write_register(0x00, 0x07, 0x00) != Aic3204Status::OK) {
    return; // D=0 MSB
  }
  if (write_register(0x00, 0x08, 0x00) != Aic3204Status::OK) {
    return; // D=0 LSB
  }
  if (write_register(0x00, 0x0B, 0x85) != Aic3204Status::OK) {
    return; // NDAC = 5, ON
  }
  if (write_register(0x00, 0x0C, 0x83) != Aic3204Status::OK) {
    return; // MDAC = 3, ON
  }
  if (write_register(0x00, 0x0D, 0x00) != Aic3204Status::OK) {
    return; // DOSR = 128 MSB
  }
  if (write_register(0x00, 0x0E, 0x80) != Aic3204Status::OK) {
    return; // DOSR = 128 LSB
  }

  // Audio Interface Settings (Page 0)
  if (write_register(0x00, 0x1B, 0x00) != Aic3204Status::OK) {
    return; // I2S, 16 bit
  }
  if (write_register(0x00, 0x19, 0x00) != Aic3204Status::OK) {
    return; // BCLK/WCLK inputs
  }
  if (write_register(0x00, 0x38, 0x04) != Aic3204Status::OK) {
    return; // Enable MFP3 as GPIO input
  }

  // DAC Processing Block (Page 0)
  if (write_register(0x00, 0x3C, 0x08) != Aic3204Status::OK) {
    return; // DAC PRB_P8
  }

  if (select_page(1) != Aic3204Status::OK) {
    return;
  }
  // --- Power and Analog Configuration (Page 1) ---
  // Power Settings
  if (write_register(0x01, 0x01, 0x08) != Aic3204Status::OK) {
    return; // Disable Crude AVdd
  }
  if (write_register(0x01, 0x02, 0x00) != Aic3204Status::OK) {
    return; // Analog Blocks OFF
  }
  if (write_register(0x01, 0x02, 0x01) != Aic3204Status::OK) {
    return; // Master Analog ON, AVDD LDO ON
  }

  if (write_register(0x01, 0x0A, 0x33) != Aic3204Status::OK) {
    return; // HP CM=1.65V, Lineout CM=0.9V, LDO=1.72V
  }

  // DAC/ADC PTM modes (Page 1)
  if (write_register(0x01, 0x03, 0x00) != Aic3204Status::OK) {
    return; // DAC PTM = P3/4
  }
  if (write_register(0x01, 0x04, 0x00) != Aic3204Status::OK) {
    return; // ADC PTM = R4
  }

  // Power-up Timing (Page 1)
  if (write_register(0x01, 0x47, 0x32) != Aic3204Status::OK) {
    return; // Input power-up time 3.1ms
  }
  if (write_register(0x01, 0x7B, 0x01) != Aic3204Status::OK) {
    return; // REF charging time 40ms
  }

  // --- Output Driver Configuration (Page 1) ---
  // Headphone Routing & Gain (0dB)
  if (write_register(0x01, 0x14, 0x05) != Aic3204Status::OK) {
    return; // Slowly ramp up HP drivers
  }
  if (write_register(0x01, 0x0C, 0x0A) != Aic3204Status::OK) {
    return; // DAC_L -> HPL, MAL -> HPL
  }
  if (write_register(0x01, 0x0D, 0x0A) != Aic3204Status::OK) {
    return; // DAC_R -> HPR, MAR -> HPR
  }
  if (write_register(0x01, 0x10, 0x00) != Aic3204Status::OK) {
    return; // HPL Gain 0dB
  }
  if (write_register(0x01, 0x11, 0x00) != Aic3204Status::OK) {
    return; // HPR Gain 0dB
  }

  // Line Output Routing & Gain (0dB Gain)
  if (write_register(0x01, 0x0E, 0x03) != Aic3204Status::OK) {
    return; // LOL Diff Config, MAL to LOL
  }
  if (write_register(0x01, 0x0F, 0x08) != Aic3204Status::OK) {
    return; // LOR Diff Config
  }
  if (write_register(0x01, 0x12, 0x00) != Aic3204Status::OK) {
    return; // LOL Gain 0dB
  }
  if (write_register(0x01, 0x13, 0x00) != Aic3204Status::OK) {
    return; // LOR Gain 0dB
  }

  // Route Line In to PGA's and then to MAL/MAR
  if (write_register(0x01, 0x34, 0x40) != Aic3204Status::OK) {
    return; // IN1L is routed to Left MICPGA with 10k resistance
  }
  if (write_register(0x01, 0x36, 0x40) != Aic3204Status::OK) {
    return; // CM is routed to Left MICPGA via CM1L with 10k resistance
  }
  if (write_register(0x01, 0x37, 0x40) != Aic3204Status::OK) {
    return; // IN1R is routed to Right MICPGA with 10k resistance
  }
  if (write_register(0x01, 0x39, 0x40) != Aic3204Status::OK) {
    return; // CM is routed to Right MICPGA via CM1R with 10k resistance
  }

  // Set MICPGA Volume Control for 8.5dB boost on line inputs
  if (write_register(0x01, 0x3B, 0x11) != Aic3204Status::OK) {
    return; // Left MICPGA Volume: +8.5dB (0.5dB steps)
  }
  if (write_register(0x01, 0x3C, 0x11) != Aic3204Status::OK) {
    return; // Right MICPGA Volume: +8.5dB (0.5dB steps)
  }

  // Power up Output Drivers (Page 1) - This starts soft-stepping
  if (write_register(0x01, 0x09, 0x3F) != Aic3204Status::OK) {
    return; // Power up HPL, HPR, LOL, LOR, MAL, MAR
  }

  // --- Wait for soft-stepping completion ---
  if (wait_for_soft_stepping() == Aic3204Status::ERROR_STEPPING_TIMEOUT) {
    AIC_LOG("AIC3204 Warning: Timeout waiting for soft-stepping completion.");
  }

  // --- Final DAC Setup (Page 0) ---
  if (write_register(0x00, 0x00, 0x00) != Aic3204Status::OK) {
    return; // Select Page 0
  }
  if (write_register(0x00, 0x3F, 0xD6) != Aic3204Status::OK) {
    return; // Power up L&R DAC Channels (Digital)
  }
  if (write_register(0x00, 0x40, 0x00) != Aic3204Status::OK) {
    return; // Unmute DAC digital volume, 0dB gain
  }
  _dac_muted = false; // DAC is now unmuted

  _is_initialized = true;
  AIC_LOG("AIC3204 register initialization complete.");

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
    AIC_LOG("AIC3204 De-initialized.");
  }

  if (_reset_pin != 0xFF) {
    gpio_put(_reset_pin, 0); // Set LOW
    gpio_set_function(_reset_pin, GPIO_FUNC_NULL);
    gpio_disable_pulls(_reset_pin);
  }
}

bool Aic3204::is_initialized() const {
  return _is_initialized;
}

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
    AIC_LOG("AIC3204 Error: Failed writing value 0x%02X to Page %d, Reg 0x%02X",
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
    AIC_LOG("AIC3204 Warning: Reading Page 0, Reg 0 (Page Select) might not be "
            "meaningful.");
  }

  Aic3204Status status = select_page(page);
  if (status != Aic3204Status::OK) {
    return status;
  }

  status = i2c_read(reg_addr, read_value);
  if (status != Aic3204Status::OK) {
    AIC_LOG("AIC3204 Error: Failed reading from Page %d, Reg 0x%02X",
            _current_page, reg_addr);
    return status;
  }

  return Aic3204Status::OK;
}

Aic3204Status Aic3204::set_amp_enabled(bool enable) {
  if (!is_initialized())
    return Aic3204Status::ERROR_NOT_INITIALIZED;

  if (AMP_ENABLE_THROUGH_CODEC) {
    AIC_LOG("%s external AMP via Codec GPIO MFP4 (%s)...",
            enable ? "Enabling" : "Disabling", enable ? "HIGH" : "LOW");
    uint8_t value = enable ? 0x05 : 0x00;
    Aic3204Status status = write_register(0x00, 0x37, value);
    if (status != Aic3204Status::OK) {
      AIC_LOG("AIC3204 Warning: Failed to set MFP4 %s to %s amp.",
              enable ? "HIGH" : "LOW", enable ? "enable" : "disable");
    }
    sleep_ms(10);
    return status;
  }
  AIC_LOG("AIC3204 Warning: External AMP is not managed through the codec.");
  return Aic3204Status::OK;
}

Aic3204Status Aic3204::set_headphone_enabled(bool enable) {
  if (!is_initialized())
    return Aic3204Status::ERROR_NOT_INITIALIZED;

  AIC_LOG("%s headphone drivers (HPL/HPR)...",
          enable ? "Enabling" : "Disabling");

  // Register 0x01/0x09: Output Driver Power Control
  // Bit positions: D5=HPL, D4=HPR, D3=LOL, D2=LOR, D1=MAL, D0=MAR
  // 0x3F = 00111111 (all outputs enabled)
  // Disable only HPL/HPR: turn off bits D5,D4 but keep D3,D2,D1,D0
  // Result: 00001111 = 0x0F (MAL/MAR/LOL/LOR stay enabled)
  uint8_t power_reg_value = enable ? 0x3F : 0x0F;

  Aic3204Status status = write_register(0x01, 0x09, power_reg_value);
  if (status != Aic3204Status::OK) {
    AIC_LOG("AIC3204 Error: Failed to %s headphone drivers.",
            enable ? "enable" : "disable");
    return status;
  }

  // Wait for soft-stepping completion when powering up
  if (enable) {
    if (wait_for_soft_stepping() == Aic3204Status::ERROR_STEPPING_TIMEOUT) {
      AIC_LOG("AIC3204 Warning: Timeout waiting for headphone power-up "
              "soft-stepping.");
    }
  }

  return Aic3204Status::OK;
}

Aic3204Status Aic3204::set_dac_volume(int8_t volume) {
  if (!is_initialized())
    return Aic3204Status::ERROR_NOT_INITIALIZED;

  if (volume < -127 || volume > 48) {
    AIC_LOG("AIC3204 Error: DAC volume %d invalid. Valid range: -127 to +48",
            volume);
    return Aic3204Status::ERROR_INVALID_ARG;
  }

  if (volume == _current_dac_volume) {
    return Aic3204Status::OK;
  }

  if (is_soft_stepping()) {
    AIC_LOG("AIC3204 Warning: Cannot set DAC volume while soft-stepping is "
            "active.");
    return Aic3204Status::ERROR_STEPPING_ACTIVE;
  }

  uint8_t reg_value = static_cast<uint8_t>(volume);
  Aic3204Status status_l = write_register(0x00, 0x41, reg_value);
  Aic3204Status status_r = write_register(0x00, 0x42, reg_value);

  if (status_l == Aic3204Status::OK && status_r == Aic3204Status::OK) {
    AIC_LOG("AIC3204: DAC volume set to %+d (%.1fdB)", volume, volume * 0.5f);
    _current_dac_volume = volume;
    return Aic3204Status::OK;
  } else {
    AIC_LOG("AIC3204 Error: Failed to write DAC volume registers");
    _current_dac_volume = INT8_MIN; // Invalidate cache
    return (status_l != Aic3204Status::OK) ? status_l : status_r;
  }
}

Aic3204Status Aic3204::set_mixer_volume(int8_t volume) {
  if (!is_initialized())
    return Aic3204Status::ERROR_NOT_INITIALIZED;

  // The user provides a value from 0 (0dB) to -40 (Mute).
  if (volume > 0 || volume < -40) {
    AIC_LOG("AIC3204 Error: Mixer volume %d invalid. Valid range: 0 to -40.",
            volume);
    return Aic3204Status::ERROR_INVALID_ARG;
  }

  if (volume == _current_mixer_volume) {
    return Aic3204Status::OK;
  }

  // These registers are on Page 1
  const uint8_t MIXER_PAGE = 1;
  const uint8_t LEFT_MIXER_REG = 0x18;
  const uint8_t RIGHT_MIXER_REG = 0x19;

  // Convert the user-facing volume (0 to -40) to the register value (0 to 40).
  uint8_t reg_value = static_cast<uint8_t>(-volume);

  Aic3204Status status_l =
      write_register(MIXER_PAGE, LEFT_MIXER_REG, reg_value);
  Aic3204Status status_r =
      write_register(MIXER_PAGE, RIGHT_MIXER_REG, reg_value);

  if (status_l == Aic3204Status::OK && status_r == Aic3204Status::OK) {
    AIC_LOG("AIC3204: Mixer volume set to attenuation step %d", volume);
    _current_mixer_volume = volume;
    return Aic3204Status::OK;
  } else {
    AIC_LOG("AIC3204 Error: Failed to write mixer volume registers");
    _current_mixer_volume = INT8_MIN; // Invalidate cache
    return (status_l != Aic3204Status::OK) ? status_l : status_r;
  }
}

Aic3204Status Aic3204::set_dac_muted(bool muted) {
  if (!is_initialized()) {
    return Aic3204Status::ERROR_NOT_INITIALIZED;
  }

  if (muted == _dac_muted) {
    return Aic3204Status::OK;
  }

  const uint8_t PAGE = 0;
  const uint8_t DAC_SETUP_REG = 0x40;
  const uint8_t LEFT_DAC_MUTE_BIT = (1 << 3);
  const uint8_t RIGHT_DAC_MUTE_BIT = (1 << 2);

  uint8_t reg_value = 0;
  Aic3204Status status = read_register(PAGE, DAC_SETUP_REG, reg_value);
  if (status != Aic3204Status::OK) {
    AIC_LOG(
        "AIC3204 Error: Failed to read DAC setup register for mute control");
    return status;
  }

  if (muted) {
    reg_value |= (LEFT_DAC_MUTE_BIT | RIGHT_DAC_MUTE_BIT);
  } else {
    reg_value &= ~(LEFT_DAC_MUTE_BIT | RIGHT_DAC_MUTE_BIT);
  }

  status = write_register(PAGE, DAC_SETUP_REG, reg_value);
  if (status != Aic3204Status::OK) {
    AIC_LOG("AIC3204 Error: Failed to write DAC mute control");
    return status;
  }

  _dac_muted = muted;
  AIC_LOG("AIC3204: DAC %s", muted ? "muted" : "unmuted");
  return Aic3204Status::OK;
}

std::optional<bool> Aic3204::is_headphone_inserted() {
  if (!is_initialized()) {
    return std::nullopt;
  }

  // Read MFP3 pin state directly from GPIO control register
  // Page 0, Reg 0x36: MFP3 Pin Control/Status
  // Bit 0 contains the pin level when configured as input
  const uint8_t PAGE = 0;
  const uint8_t MFP3_REG = 0x36;
  const uint8_t PIN_LEVEL_MASK = (1 << 0);

  uint8_t reg_val = 0;
  Aic3204Status status = read_register(PAGE, MFP3_REG, reg_val);
  if (status != Aic3204Status::OK) {
    AIC_LOG("AIC3204 Error: Failed to read MFP3 pin state.");
    return std::nullopt;
  }

  // MFP3 pin is typically pulled HIGH by default and goes LOW when headphones
  // are inserted (jack switch grounds the pin). Invert the logic.
  bool pin_low = (reg_val & PIN_LEVEL_MASK) == 0;
  return pin_low; // true = headphones inserted (pin LOW)
}

Aic3204Status Aic3204::enter_sleep_mode() {
  if (!is_initialized()) {
    return Aic3204Status::ERROR_NOT_INITIALIZED;
  }

  AIC_LOG("Entering sleep mode (3.3V operation)...");

  // Page 1, Register 1, D(3) = 0: Turn on crude AVDD generation
  uint8_t reg1_val;
  if (read_register(0x01, 0x01, reg1_val) != Aic3204Status::OK) {
    return Aic3204Status::ERROR_I2C_READ_FAILED;
  }
  reg1_val &= ~(1 << 3); // Clear bit 3
  if (write_register(0x01, 0x01, reg1_val) != Aic3204Status::OK) {
    return Aic3204Status::ERROR_I2C_WRITE_FAILED;
  }

  // Page 1, Register 2, D(0) = 0: Power down AVDD LDO
  // Page 1, Register 2, D(3) = 1: Power down analog blocks
  uint8_t reg2_val;
  if (read_register(0x01, 0x02, reg2_val) != Aic3204Status::OK) {
    return Aic3204Status::ERROR_I2C_READ_FAILED;
  }
  reg2_val &= ~(1 << 0); // Clear bit 0 (power down AVDD LDO)
  reg2_val |= (1 << 3);  // Set bit 3 (power down analog blocks)
  if (write_register(0x01, 0x02, reg2_val) != Aic3204Status::OK) {
    return Aic3204Status::ERROR_I2C_WRITE_FAILED;
  }

  AIC_LOG("Sleep mode entered successfully");
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
      AIC_LOG("AIC3204 Error: Failed to select page %d", page);
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
    AIC_LOG("AIC3204 Warning: Failed to read soft-stepping status register. "
            "Assuming active.");
    return true; // Fail-safe
  }
}

Aic3204Status Aic3204::wait_for_soft_stepping() {
  AIC_LOG("Waiting for codec soft-stepping completion (max %d ms)...",
          SOFT_STEPPING_TIMEOUT_MS);
  absolute_time_t timeout_time = make_timeout_time_ms(SOFT_STEPPING_TIMEOUT_MS);

  while (!time_reached(timeout_time)) {
    if (!is_soft_stepping()) {
      AIC_LOG("Soft-stepping complete.");
      return Aic3204Status::OK;
    }
    sleep_ms(10);
  }

  return Aic3204Status::ERROR_STEPPING_TIMEOUT;
}

} // namespace musin::drivers

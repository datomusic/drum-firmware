#ifndef MUSIN_DRIVERS_AIC3204_HPP
#define MUSIN_DRIVERS_AIC3204_HPP

#include <cstdint>
#include <optional>

// Forward declare I2C instance type to avoid including hardware headers here
struct i2c_inst;
typedef struct i2c_inst i2c_inst_t;

namespace musin::drivers {

/**
 * @brief Status codes for AIC3204 operations.
 */
enum class Aic3204Status {
  OK = 0,
  ERROR_NOT_INITIALIZED,
  ERROR_I2C_WRITE_FAILED,
  ERROR_I2C_READ_FAILED,
  ERROR_DEVICE_NOT_FOUND,
  ERROR_INVALID_PINS,
  ERROR_INVALID_ARG,
  ERROR_STEPPING_TIMEOUT,
  ERROR_STEPPING_ACTIVE,
};

/**
 * @brief C++ driver for the TI AIC3204 Stereo Audio Codec.
 *
 * This class manages the lifecycle and configuration of the AIC3204 codec
 * using RAII. The constructor performs initialization, and the destructor
 * handles cleanup.
 */
class Aic3204 {
public:
  /**
   * @brief Constructs and initializes the AIC3204 codec.
   *
   * After construction, call is_initialized() to verify success.
   *
   * @param sda_pin The GPIO pin number for I2C SDA.
   * @param scl_pin The GPIO pin number for I2C SCL.
   * @param baudrate The I2C communication speed in Hz.
   * @param reset_pin Optional GPIO pin for hardware reset.
   */
  Aic3204(uint8_t sda_pin, uint8_t scl_pin, uint32_t baudrate, uint8_t reset_pin = 0xFF);

  /**
   * @brief Destructor. De-initializes the I2C peripheral and GPIO pins.
   */
  ~Aic3204();

  // Prevent copying/moving to ensure single ownership of the hardware.
  Aic3204(const Aic3204 &) = delete;
  Aic3204 &operator=(const Aic3204 &) = delete;
  Aic3204(Aic3204 &&) = delete;
  Aic3204 &operator=(Aic3204 &&) = delete;

  /**
   * @brief Checks if the codec was successfully initialized.
   * @return true if initialization succeeded, false otherwise.
   */
  bool is_initialized() const;

  // --- Public API ---
  Aic3204Status write_register(uint8_t page, uint8_t reg_addr, uint8_t value);
  Aic3204Status read_register(uint8_t page, uint8_t reg_addr, uint8_t &read_value);
  Aic3204Status set_amp_enabled(bool enable);
  Aic3204Status set_dac_volume(int8_t volume);
  Aic3204Status set_mixer_volume(int8_t volume);
  std::optional<bool> is_headphone_inserted();
  bool update_headphone_detection();

private:
  // --- Constants ---
  static constexpr uint8_t I2C_ADDR = 0x18;
  static constexpr int SOFT_STEPPING_TIMEOUT_MS = 1000;
  static constexpr bool AMP_ENABLE_THROUGH_CODEC = true;

  // --- Private Helper Methods ---
  Aic3204Status select_page(uint8_t page);
  Aic3204Status i2c_write(uint8_t reg_addr, uint8_t value);
  Aic3204Status i2c_read(uint8_t reg_addr, uint8_t &value);
  bool is_soft_stepping();
  Aic3204Status wait_for_soft_stepping();
  Aic3204Status configure_headphone_jack_detection();
  i2c_inst_t *get_i2c_instance(uint8_t sda_pin, uint8_t scl_pin);
  bool device_present(uint8_t addr);

  // --- Member Variables ---
  i2c_inst_t *_i2c_inst = nullptr;
  uint8_t _sda_pin;
  uint8_t _scl_pin;
  uint8_t _reset_pin;
  bool _is_initialized = false;
  uint8_t _current_page = 0xFF;
  int8_t _current_dac_volume = 0;
  int8_t _current_mixer_volume = 0;
  bool _headphone_inserted_state = false;
};

} // namespace musin::drivers

#endif // MUSIN_DRIVERS_AIC3204_HPP

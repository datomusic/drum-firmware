#ifndef MUSIN_DRIVERS_WS2812_H // Updated include guard name
#define MUSIN_DRIVERS_WS2812_H

#include <cstdint>
#include <vector>
#include <optional>

// Wrap C SDK headers
extern "C" {
#include "hardware/pio.h"
}

// Forward declaration for the generated PIO program header
// This avoids including the generated header here, keeping this header cleaner.
// The .cpp file will include the actual generated header.
struct pio_program;

namespace Musin::Drivers { // Updated namespace

/**
 * @brief Defines the order of Red, Green, and Blue components for WS2812 LEDs.
 */
enum class RGBOrder {
    RGB,
    RBG,
    GRB, // Most common for WS2812/NeoPixel
    GBR,
    BRG,
    BGR
};

/**
 * @brief Driver for WS2812/NeoPixel addressable LEDs using Raspberry Pi Pico PIO.
 *
 * Manages a strip of LEDs, handling color setting, brightness, optional color
 * correction, and communication via a PIO state machine.
 * Assumes the corresponding PIO program (e.g., ws2812.pio) has been compiled
 * and linked via the build system.
 */
class WS2812 {
public:
    /**
     * @brief Construct a WS2812 driver instance.
     *
     * @param pio The PIO instance to use (pio0 or pio1).
     * @param sm_index The state machine index within the PIO (0-3) to claim.
     * @param data_pin The GPIO pin connected to the WS2812 data input.
     * @param num_leds The number of LEDs in the strip.
     * @param order The color order of the LEDs (e.g., RGBOrder::GRB).
     * @param initial_brightness Optional initial brightness (0-255, default 255).
     * @param color_correction Optional color correction value (e.g., 0xFFB0F0, default none).
     */
    WS2812(unsigned int data_pin, unsigned int num_leds, // PIO and SM are now determined dynamically in init()
           RGBOrder order = RGBOrder::GRB,
           uint8_t initial_brightness = 255,
           std::optional<uint32_t> color_correction = std::nullopt);

    // Prevent copying and assignment
    WS2812(const WS2812&) = delete;
    WS2812& operator=(const WS2812&) = delete;

    /**
     * @brief Initialize the PIO state machine for WS2812 communication.
     * Must be called once before using the driver.
     * Loads the PIO program if not already loaded on this PIO instance.
     * Claims and configures the specified state machine.
     * @return true if initialization was successful, false otherwise (e.g., SM already claimed).
     */
    bool init();

    /**
     * @brief Set the color of a single pixel in the buffer.
     * Applies brightness and color correction.
     * Does not update the physical LEDs until show() is called.
     *
     * @param index The 0-based index of the pixel. Must be less than num_leds.
     * @param r Red component (0-255).
     * @param g Green component (0-255).
     * @param b Blue component (0-255).
     */
    void set_pixel(unsigned int index, uint8_t r, uint8_t g, uint8_t b);

    /**
     * @brief Set the color of a single pixel using a 24-bit RGB value.
     * Applies brightness and color correction.
     * Does not update the physical LEDs until show() is called.
     *
     * @param index The 0-based index of the pixel. Must be less than num_leds.
     * @param color The 24-bit color (e.g., 0xRRGGBB).
     */
    void set_pixel(unsigned int index, uint32_t color);

    /**
     * @brief Send the current pixel buffer data to the physical LED strip via PIO.
     * Blocks until all data is pushed to the PIO FIFO.
     */
    void show();

    /**
     * @brief Set all pixels in the buffer to black (0,0,0).
     * Does not update the physical LEDs until show() is called.
     */
    void clear();

    /**
     * @brief Set the global brightness level. Affects subsequent set_pixel() calls.
     * Does not modify the colors already in the buffer until set_pixel is called again.
     *
     * @param brightness Brightness level (0-255).
     */
    void set_brightness(uint8_t brightness);

    /**
     * @brief Get the current global brightness level.
     * @return uint8_t Brightness level (0-255).
     */
    uint8_t get_brightness() const;

    /**
     * @brief Get the number of LEDs managed by this driver.
     * @return unsigned int Number of LEDs.
     */
    unsigned int get_num_leds() const; // Already uses unsigned int, good.

private:
    /**
     * @brief Apply brightness and color correction to RGB components.
     * Internal helper used by set_pixel methods.
     *
     * @param r Input Red component.
     * @param g Input Green component.
     * @param b Input Blue component.
     * @param out_r Output Red component after adjustments.
     * @param out_g Output Green component after adjustments.
     * @param out_b Output Blue component after adjustments.
     */
    void apply_brightness_and_correction(uint8_t r, uint8_t g, uint8_t b,
                                         uint8_t& out_r, uint8_t& out_g, uint8_t& out_b) const;

    /**
     * @brief Pack RGB components into a 24-bit integer based on the configured order.
     * Internal helper used by set_pixel methods.
     *
     * @param r Red component.
     * @param g Green component.
     * @param b Blue component.
     * @return uint32_t Packed 24-bit color value in the correct order for the PIO.
     */
    uint32_t pack_color(uint8_t r, uint8_t g, uint8_t b) const;

    // --- Configuration ---
    PIO _pio;
    unsigned int _sm_index;
    unsigned int _data_pin;
    unsigned int _num_leds;
    RGBOrder _order;
    uint8_t _brightness;
    std::optional<uint32_t> _color_correction;

    // --- State ---
    std::vector<uint32_t> _pixel_buffer; // Stores packed 24-bit color values after adjustments
    unsigned int _pio_program_offset = 0;        // Offset of the loaded PIO program within the PIO instance
    bool _initialized = false;

    // --- PIO Program Info ---
    // PIO program loading and SM claiming are now handled dynamically in init()
    // using SDK helpers. Static tracking is no longer needed here.

}; // class WS2812

} // namespace Musin::Drivers // Corrected closing namespace comment

#endif // MUSIN_DRIVERS_WS2812_H // Corrected endif comment

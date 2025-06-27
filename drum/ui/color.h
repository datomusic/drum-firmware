#ifndef DRUM_COLOR_H
#define DRUM_COLOR_H

#include <algorithm>
#include <cstdint>

namespace drum {

class Color {
public:
  // Default to black
  constexpr Color() : value(0) {
  }
  // Construct from a 24-bit RGB value
  constexpr explicit Color(uint32_t rgb) : value(rgb) {
  }

  // Allow explicit conversion back to uint32_t for driver calls
  constexpr explicit operator uint32_t() const {
    return value;
  }

  // Additive blend with white
  [[nodiscard]] Color brighter(uint8_t amount, uint8_t max_brightness = 255) const {
    uint8_t r = (value >> 16) & 0xFF;
    uint8_t g = (value >> 8) & 0xFF;
    uint8_t b = value & 0xFF;

    r = static_cast<uint8_t>(std::min<int>(max_brightness, static_cast<int>(r) + amount));
    g = static_cast<uint8_t>(std::min<int>(max_brightness, static_cast<int>(g) + amount));
    b = static_cast<uint8_t>(std::min<int>(max_brightness, static_cast<int>(b) + amount));

    return Color((static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | b);
  }

  // Equality operators
  bool operator==(const Color &other) const {
    return value == other.value;
  }

  bool operator==(uint32_t other_rgb) const {
    return value == other_rgb;
  }

private:
  uint32_t value;
};

} // namespace drum

#endif // DRUM_COLOR_H

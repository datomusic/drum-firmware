#ifndef TESTMACHINE_COLOR_H
#define TESTMACHINE_COLOR_H

#include <cstdint>

namespace testmachine {
namespace ui {

class Color {
public:
  constexpr Color() : value(0) {}
  constexpr explicit Color(uint32_t rgb) : value(rgb) {}

  constexpr explicit operator uint32_t() const { return value; }

  bool operator==(const Color &other) const { return value == other.value; }
  bool operator==(uint32_t other_rgb) const { return value == other_rgb; }

private:
  uint32_t value;
};

} // namespace ui
} // namespace testmachine

#endif // TESTMACHINE_COLOR_H

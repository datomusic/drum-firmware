#ifndef DRUM_CONFIG_KIT_DEFINITIONS_H
#define DRUM_CONFIG_KIT_DEFINITIONS_H

#include <cstdint>

namespace drum {
namespace config {

struct Color {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

struct SampleSlotMetadata {
  uint8_t midi_note;
  Color color;
  uint8_t track;
  uint8_t reserved[3]; // Padding for alignment
};

} // namespace config
} // namespace drum

#endif // DRUM_CONFIG_KIT_DEFINITIONS_H

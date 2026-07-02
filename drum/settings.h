#ifndef DRUM_SETTINGS_H
#define DRUM_SETTINGS_H

#include "etl/array.h"
#include "etl/string_view.h"
#include <cstddef>
#include <cstdint>

namespace drum::settings {

/**
 * @brief Identifiers for device settings, used as keys on the SysEx wire.
 *
 * Values must stay 7-bit safe (0x01-0x7F) and must never be reused for a
 * different setting once released.
 */
enum class Id : uint8_t {
  MidiChannel = 0x01,
  SliderMode = 0x02,
};

/**
 * @brief Bit mask values for the SliderMode setting: which parameters the
 * per-track slider controls on the audio engine. Any combination is valid.
 */
namespace slider_mode {
inline constexpr uint8_t PITCH = 1u << 0;
inline constexpr uint8_t GAIN = 1u << 1;
inline constexpr uint8_t DECAY = 1u << 2;
} // namespace slider_mode

/**
 * @brief Describes one setting: wire id, short name, valid range and default.
 *
 * The name doubles as the filename under /settings/ on the device
 * filesystem, so keep it short, lowercase and filesystem-safe.
 */
struct Descriptor {
  Id id;
  etl::string_view name;
  uint8_t min;
  uint8_t max;
  uint8_t default_value;
};

inline constexpr etl::array<Descriptor, 2> DESCRIPTORS{{
    {Id::MidiChannel, "midi_channel", 1, 16, 10},
    {Id::SliderMode, "slider_mode", 0, 7, slider_mode::PITCH},
}};

/**
 * @brief Finds the descriptor for a setting id.
 * @return Pointer into DESCRIPTORS, or nullptr for unknown ids.
 */
constexpr const Descriptor *find_descriptor(Id id) {
  for (const auto &descriptor : DESCRIPTORS) {
    if (descriptor.id == id) {
      return &descriptor;
    }
  }
  return nullptr;
}

/**
 * @brief In-RAM store of all setting values.
 *
 * Values are always within their descriptor's range; set() rejects anything
 * else. Persistence is handled separately by SettingsManager.
 */
class Settings {
public:
  constexpr Settings() {
    for (size_t i = 0; i < DESCRIPTORS.size(); ++i) {
      values_[i] = DESCRIPTORS[i].default_value;
    }
  }

  [[nodiscard]] constexpr uint8_t get(Id id) const {
    const auto *descriptor = find_descriptor(id);
    if (descriptor == nullptr) {
      return 0;
    }
    return values_[index_of(descriptor)];
  }

  /**
   * @brief Sets a value after validating id and range.
   * @return false for unknown ids or out-of-range values.
   */
  constexpr bool set(Id id, uint8_t value) {
    const auto *descriptor = find_descriptor(id);
    if (descriptor == nullptr || value < descriptor->min ||
        value > descriptor->max) {
      return false;
    }
    values_[index_of(descriptor)] = value;
    return true;
  }

private:
  static constexpr size_t index_of(const Descriptor *descriptor) {
    return static_cast<size_t>(descriptor - DESCRIPTORS.data());
  }

  etl::array<uint8_t, DESCRIPTORS.size()> values_{};
};

} // namespace drum::settings

#endif // DRUM_SETTINGS_H

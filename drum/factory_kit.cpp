#include "config.h"
#include "etl/array.h"
#include "etl/span.h"
#include "kit_definitions.h"

namespace drum {
namespace config {

namespace {
constexpr etl::array<SampleSlotMetadata, global_note_definitions.size()> create_factory_kit_data() {
  etl::array<SampleSlotMetadata, global_note_definitions.size()> data{};
  for (size_t i = 0; i < global_note_definitions.size(); ++i) {
    const auto &note_def = global_note_definitions[i];
    data[i] = {.midi_note = note_def.midi_note_number,
               .color = {.r = static_cast<uint8_t>((note_def.color >> 16) & 0xFF),
                         .g = static_cast<uint8_t>((note_def.color >> 8) & 0xFF),
                         .b = static_cast<uint8_t>(note_def.color & 0xFF)},
               .track = static_cast<uint8_t>(i / 8),
               .reserved = {0, 0, 0}};
  }
  return data;
}
} // namespace

const volatile auto factory_kit_data = create_factory_kit_data();

} // namespace config
} // namespace drum

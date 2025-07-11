#include "drum/drumpad_factory.h"

namespace drum {
namespace DrumpadFactory {

std::array<musin::ui::Drumpad, config::NUM_DRUMPADS> create_drumpads() {
  return {{
      musin::ui::Drumpad(0, config::drumpad::drumpad_configs[0]),
      musin::ui::Drumpad(1, config::drumpad::drumpad_configs[1]),
      musin::ui::Drumpad(2, config::drumpad::drumpad_configs[2]),
      musin::ui::Drumpad(3, config::drumpad::drumpad_configs[3]),
  }};
}

} // namespace DrumpadFactory
} // namespace drum

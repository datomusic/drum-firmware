#include "drum/drumpad_factory.h"

namespace drum {
namespace DrumpadFactory {

std::array<musin::ui::Drumpad, config::NUM_DRUMPADS> create_drumpads() {
  return {{
      musin::ui::Drumpad{/*pad_id=*/0},
      musin::ui::Drumpad{/*pad_id=*/1},
      musin::ui::Drumpad{/*pad_id=*/2},
      musin::ui::Drumpad{/*pad_id=*/3}
  }};
}

} // namespace DrumpadFactory
} // namespace drum

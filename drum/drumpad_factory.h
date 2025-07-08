#ifndef DRUM_DRUMPAD_FACTORY_H
#define DRUM_DRUMPAD_FACTORY_H

#include "drum/config.h"
#include "musin/ui/drumpad.h"
#include <array>

namespace drum {

namespace DrumpadFactory {

/**
 * @brief Creates an array of Drumpad objects at compile time.
 *
 * This factory initializes all drumpads with their default configurations,
 * assigning a unique ID to each. This allows for a clean separation of
 * configuration from the component that uses the drumpads.
 *
 * @return An std::array containing the configured Drumpad objects.
 */
std::array<musin::ui::Drumpad, config::NUM_DRUMPADS> create_drumpads();

} // namespace DrumpadFactory
} // namespace drum

#endif // DRUM_DRUMPAD_FACTORY_H

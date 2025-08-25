#ifndef I_CLOCK_OBSERVER_H
#define I_CLOCK_OBSERVER_H

#include "etl/observer.h"
#include "musin/timing/tempo_event.h"

namespace musin {
namespace timing {

using I_ClockObserver = etl::observer<TempoEvent>;

}
} // namespace musin

#endif
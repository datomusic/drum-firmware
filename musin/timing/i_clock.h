#ifndef I_CLOCK_H
#define I_CLOCK_H

#include "etl/observer.h"
#include "musin/timing/tempo_event.h"

namespace musin {
namespace timing {

class I_Clock : public etl::observable<etl::observer<TempoEvent>, 1> {
public:
  virtual ~I_Clock() = default;
  virtual void start() = 0;
  virtual void stop() = 0;
  virtual bool is_running() const = 0;
  virtual void set_bpm(float bpm) = 0;
  virtual float get_bpm() const = 0;
};

} // namespace timing
} // namespace musin

#endif
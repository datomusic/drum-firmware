
#include "etl/array.h"
struct SampleData {
  const int16_t *data;
  const uint32_t length;
};

extern const etl::array<SampleData, 32> all_samples;

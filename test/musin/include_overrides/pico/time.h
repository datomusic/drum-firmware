#ifndef MOCK_PICO_TIME_H
#define MOCK_PICO_TIME_H

#include <cstdint> // For uint64_t, int64_t

// Define absolute_time_t as it's used by the code under test and the mock functions.
typedef uint64_t absolute_time_t;

// This global variable will be defined in the test .cpp file (e.g., midi_message_queue_test.cpp)
// and controlled by the tests to simulate time.
extern absolute_time_t mock_current_time;

// Define nil_time and at_the_end_of_time as they are used by pico/time.h and potentially by code
// including it. These values are taken from the actual pico/time.h.
#define nil_time ((absolute_time_t)0)
#define at_the_end_of_time ((absolute_time_t)0xffffffffffffffffULL)

// Mock implementation for get_absolute_time()
// Returns the current value of the global mock_current_time.
static inline absolute_time_t get_absolute_time() {
  return mock_current_time;
}

// Mock implementation for is_nil_time()
// Checks if the given time is nil_time.
static inline bool is_nil_time(absolute_time_t t) {
  return t == nil_time;
}

// Mock implementation for absolute_time_diff_us()
// Calculates the difference in microseconds between two absolute_time_t values.
// Matches the signature from pico/time.h which returns int64_t.
static inline int64_t absolute_time_diff_us(absolute_time_t from, absolute_time_t to) {
  if (is_nil_time(from)) {
    // Behavior for nil 'from' can vary; often results in a large positive or negative
    // value if 'to' is not nil, or 0 if both are nil.
    // The actual SDK might have specific behavior; for testing, returning 0 if 'from' is nil
    // is a simple approach, or one could try to mimic the SDK's large value if 'to' is far.
    // Given the queue logic, if last_non_realtime_send_time is nil, it implies a send is allowed.
    return is_nil_time(to) ? 0 : static_cast<int64_t>(to); // Simplified: if from is nil, effectively infinite time has passed.
  }
  if (is_nil_time(to)) {
      // If 'to' is nil and 'from' is not, this implies a negative duration.
      return -static_cast<int64_t>(from);
  }
  return static_cast<int64_t>(to) - static_cast<int64_t>(from);
}

// Helper functions for tests to control mock time.
// These are static inline and can live in the mock header for convenience.
static inline void advance_mock_time_us(uint64_t us) {
  mock_current_time += us;
}

static inline void set_mock_time_us(uint64_t us) {
  mock_current_time = us;
}

#endif // MOCK_PICO_TIME_H

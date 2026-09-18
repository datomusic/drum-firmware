#ifndef TESTMACHINE_TEST_FRAMEWORK_TEST_INTERFACE_H
#define TESTMACHINE_TEST_FRAMEWORK_TEST_INTERFACE_H

#include "etl/string.h"
#include <cstdint>

extern "C" {
#include "pico/time.h"
}

namespace testmachine {

enum class TestStatus : uint8_t {
  NotStarted,
  Running,
  Passed,
  Failed,
  Timeout
};

struct TestResult {
  TestStatus status = TestStatus::NotStarted;
  etl::string<64> message;

  static TestResult passed(const char *msg = "passed") {
    return {TestStatus::Passed, etl::string<64>(msg)};
  }

  static TestResult failed(const char *msg) {
    return {TestStatus::Failed, etl::string<64>(msg)};
  }

  static TestResult timeout(const char *msg = "timeout") {
    return {TestStatus::Timeout, etl::string<64>(msg)};
  }
};

class Test {
public:
  virtual ~Test() = default;

  virtual const char *get_name() const = 0;

  virtual void start(absolute_time_t now) = 0;

  virtual void update(absolute_time_t now) = 0;

  virtual bool is_complete() const = 0;

  virtual TestResult get_result() const = 0;

  virtual void reset() = 0;
};

} // namespace testmachine

#endif // TESTMACHINE_TEST_FRAMEWORK_TEST_INTERFACE_H

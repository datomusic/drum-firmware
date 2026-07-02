#include "response_formatter.h"

#include "version.h"

#include <cstdio>

namespace testmachine {

namespace {

const char *status_to_string(TestStatus status) {
  switch (status) {
  case TestStatus::NotStarted:
    return "not_started";
  case TestStatus::Running:
    return "running";
  case TestStatus::Passed:
    return "pass";
  case TestStatus::Failed:
    return "fail";
  case TestStatus::Timeout:
    return "timeout";
  }
  return "unknown";
}

} // namespace

void ResponseFormatter::send_pong() {
  printf("PONG:testmachine-%s\n", FIRMWARE_VERSION);
}

void ResponseFormatter::send_version() {
  printf("VERSION:%s\n", FIRMWARE_VERSION);
}

void ResponseFormatter::send_test_list(const TestManager &manager) {
  printf("TESTS:");
  for (size_t i = 0; i < manager.get_test_count(); ++i) {
    if (i > 0) {
      printf(",");
    }
    printf("%s", manager.get_test(i)->get_name());
  }
  printf("\n");
}

void ResponseFormatter::send_result(const char *test_name,
                                    const TestResult &result) {
  printf("RESULT:{\"test\":\"%s\",\"status\":\"%s\",\"message\":\"%s\"}\n",
         test_name, status_to_string(result.status), result.message.c_str());
}

void ResponseFormatter::send_error(const char *message) {
  printf("ERROR:%s\n", message);
}

void ResponseFormatter::send_ok(const char *message) {
  if (message != nullptr) {
    printf("OK:%s\n", message);
  } else {
    printf("OK\n");
  }
}

void ResponseFormatter::send_busy(const char *test_name) {
  printf("BUSY:%s\n", test_name);
}

} // namespace testmachine

#ifndef TESTMACHINE_COMMAND_RESPONSE_FORMATTER_H
#define TESTMACHINE_COMMAND_RESPONSE_FORMATTER_H

#include "testmachine/test_framework/test_interface.h"
#include "testmachine/test_framework/test_manager.h"

namespace testmachine {

class ResponseFormatter {
public:
  static void send_pong();

  static void send_version();

  static void send_test_list(const TestManager &manager);

  static void send_result(const char *test_name, const TestResult &result);

  static void send_error(const char *message);

  static void send_ok(const char *message = nullptr);

  static void send_busy(const char *test_name);
};

} // namespace testmachine

#endif // TESTMACHINE_COMMAND_RESPONSE_FORMATTER_H

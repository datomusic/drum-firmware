#ifndef TESTMACHINE_TEST_FRAMEWORK_TEST_MANAGER_H
#define TESTMACHINE_TEST_FRAMEWORK_TEST_MANAGER_H

#include "test_interface.h"

#include "etl/array.h"
#include "etl/string_view.h"
#include <cstdint>

namespace testmachine {

class TestManager {
public:
  static constexpr size_t MAX_TESTS = 8;

  TestManager() = default;

  void register_test(Test *test);

  bool start_test(etl::string_view name);

  bool start_test(Test *test);

  void update(absolute_time_t now);

  [[nodiscard]] bool is_test_running() const;

  [[nodiscard]] bool is_test_complete() const;

  [[nodiscard]] Test *get_active_test() const;

  [[nodiscard]] TestResult get_result() const;

  void clear_result();

  [[nodiscard]] size_t get_test_count() const;

  [[nodiscard]] Test *get_test(size_t index) const;

  [[nodiscard]] Test *find_test(etl::string_view name) const;

private:
  etl::array<Test *, MAX_TESTS> tests_{};
  size_t test_count_ = 0;
  Test *active_test_ = nullptr;
  bool result_pending_ = false;
};

} // namespace testmachine

#endif // TESTMACHINE_TEST_FRAMEWORK_TEST_MANAGER_H

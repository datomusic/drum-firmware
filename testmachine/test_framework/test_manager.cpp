#include "test_manager.h"

#include <cstring>

namespace testmachine {

void TestManager::register_test(Test *test) {
  if (test_count_ < MAX_TESTS && test != nullptr) {
    tests_[test_count_++] = test;
  }
}

bool TestManager::start_test(etl::string_view name) {
  Test *test = find_test(name);
  if (test == nullptr) {
    return false;
  }
  return start_test(test);
}

bool TestManager::start_test(Test *test) {
  if (test == nullptr || is_test_running()) {
    return false;
  }

  test->reset();
  test->start(get_absolute_time());
  active_test_ = test;
  result_pending_ = false;
  return true;
}

void TestManager::update(absolute_time_t now) {
  if (active_test_ == nullptr) {
    return;
  }

  active_test_->update(now);

  if (active_test_->is_complete()) {
    result_pending_ = true;
  }
}

bool TestManager::is_test_running() const {
  return active_test_ != nullptr && !active_test_->is_complete();
}

bool TestManager::is_test_complete() const { return result_pending_; }

Test *TestManager::get_active_test() const { return active_test_; }

TestResult TestManager::get_result() const {
  if (active_test_ == nullptr) {
    return {TestStatus::NotStarted, "no test"};
  }
  return active_test_->get_result();
}

void TestManager::clear_result() {
  result_pending_ = false;
  active_test_ = nullptr;
}

size_t TestManager::get_test_count() const { return test_count_; }

Test *TestManager::get_test(size_t index) const {
  if (index >= test_count_) {
    return nullptr;
  }
  return tests_[index];
}

Test *TestManager::find_test(etl::string_view name) const {
  for (size_t i = 0; i < test_count_; ++i) {
    if (name == tests_[i]->get_name()) {
      return tests_[i];
    }
  }
  return nullptr;
}

} // namespace testmachine

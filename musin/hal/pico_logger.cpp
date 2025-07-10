#include "pico_logger.h"

namespace musin {

const char *PicoLogger::level_to_string(LogLevel level) const {
  switch (level) {
  case LogLevel::DEBUG:
    return "DEBUG";
  case LogLevel::INFO:
    return "INFO ";
  case LogLevel::WARN:
    return "WARN ";
  case LogLevel::ERROR:
    return "ERROR";
  default:
    return "UNKN ";
  }
}

bool PicoLogger::should_log(LogLevel level) const {
  return level >= current_level_;
}

PicoLogger::PicoLogger(LogLevel level) : current_level_(level) {
}

void PicoLogger::log(LogLevel level, etl::string_view message) {
  if (!should_log(level)) {
    return;
  }
  printf("[%s] %.*s\n", level_to_string(level), static_cast<int>(message.size()), message.data());
}

void PicoLogger::log(LogLevel level, etl::string_view message, std::int32_t value) {
  if (!should_log(level)) {
    return;
  }
  printf("[%s] %.*s: %ld\n", level_to_string(level), static_cast<int>(message.size()),
         message.data(), value);
}

void PicoLogger::log(LogLevel level, etl::string_view message, std::uint32_t value) {
  if (!should_log(level)) {
    return;
  }
  printf("[%s] %.*s: %lu\n", level_to_string(level), static_cast<int>(message.size()),
         message.data(), value);
}

void PicoLogger::log(LogLevel level, etl::string_view message, float value) {
  if (!should_log(level)) {
    return;
  }
  printf("[%s] %.*s: %.2f\n", level_to_string(level), static_cast<int>(message.size()),
         message.data(), value);
}

void PicoLogger::set_level(LogLevel level) {
  current_level_ = level;
}

LogLevel PicoLogger::get_level() const {
  return current_level_;
}

} // namespace musin
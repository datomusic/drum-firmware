#ifndef PICO_LOGGER_H
#define PICO_LOGGER_H

#include "logger.h"

extern "C" {
#include "pico/stdlib.h"
}
#include <cstdio>

namespace musin {

class PicoLogger : public Logger {
private:
  LogLevel current_level_;

  const char *level_to_string(LogLevel level) const;
  bool should_log(LogLevel level) const;

public:
  explicit PicoLogger(LogLevel level = LogLevel::INFO);

  void log(LogLevel level, etl::string_view message) override;
  void log(LogLevel level, etl::string_view message, std::int32_t value) override;
  void log(LogLevel level, etl::string_view message, std::uint32_t value) override;
  void log(LogLevel level, etl::string_view message, float value) override;

  void set_level(LogLevel level) override;
  LogLevel get_level() const override;
};

} // namespace musin
#endif
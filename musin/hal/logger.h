#ifndef LOGGER_H
#define LOGGER_H

#include <cstdint>
#include "etl/string_view.h"

extern "C" {
#include "pico/stdlib.h"
}

namespace musin {

enum class LogLevel : std::uint8_t {
  DEBUG = 0,
  INFO = 1,
  WARN = 2,
  ERROR = 3,
  NONE = 4
};

class Logger {
public:
  virtual ~Logger() = default;
  
  virtual void log(LogLevel level, etl::string_view message) = 0;
  virtual void log(LogLevel level, etl::string_view message, std::int32_t value) = 0;
  virtual void log(LogLevel level, etl::string_view message, std::uint32_t value) = 0;
  virtual void log(LogLevel level, etl::string_view message, float value) = 0;
  
  virtual void set_level(LogLevel level) = 0;
  virtual LogLevel get_level() const = 0;
  
  void debug(etl::string_view message) { log(LogLevel::DEBUG, message); }
  void info(etl::string_view message) { log(LogLevel::INFO, message); }
  void warn(etl::string_view message) { log(LogLevel::WARN, message); }
  void error(etl::string_view message) { log(LogLevel::ERROR, message); }
  
  void debug(etl::string_view message, std::int32_t value) { 
    log(LogLevel::DEBUG, message, value); 
  }
  void info(etl::string_view message, std::int32_t value) { 
    log(LogLevel::INFO, message, value); 
  }
  void warn(etl::string_view message, std::int32_t value) { 
    log(LogLevel::WARN, message, value); 
  }
  void error(etl::string_view message, std::int32_t value) { 
    log(LogLevel::ERROR, message, value); 
  }
};

class PicoLogger : public Logger {
private:
  LogLevel current_level_;
  
  const char* level_to_string(LogLevel level) const {
    switch (level) {
      case LogLevel::DEBUG: return "DEBUG";
      case LogLevel::INFO:  return "INFO ";
      case LogLevel::WARN:  return "WARN ";
      case LogLevel::ERROR: return "ERROR";
      default:              return "UNKN ";
    }
  }
  
  bool should_log(LogLevel level) const {
    return level >= current_level_;
  }
  
public:
  explicit PicoLogger(LogLevel level = LogLevel::INFO) : current_level_(level) {}
  
  void log(LogLevel level, etl::string_view message) override {
    if (!should_log(level)) {
      return;
    }
    printf("[%s] %.*s\n", level_to_string(level), 
           static_cast<int>(message.size()), message.data());
  }
  
  void log(LogLevel level, etl::string_view message, std::int32_t value) override {
    if (!should_log(level)) {
      return;
    }
    printf("[%s] %.*s: %ld\n", level_to_string(level),
           static_cast<int>(message.size()), message.data(), value);
  }
  
  void log(LogLevel level, etl::string_view message, std::uint32_t value) override {
    if (!should_log(level)) {
      return;
    }
    printf("[%s] %.*s: %lu\n", level_to_string(level),
           static_cast<int>(message.size()), message.data(), value);
  }
  
  void log(LogLevel level, etl::string_view message, float value) override {
    if (!should_log(level)) {
      return;
    }
    printf("[%s] %.*s: %.2f\n", level_to_string(level),
           static_cast<int>(message.size()), message.data(), value);
  }
  
  void set_level(LogLevel level) override {
    current_level_ = level;
  }
  
  LogLevel get_level() const override {
    return current_level_;
  }
};

class NullLogger : public Logger {
public:
  void log(LogLevel, etl::string_view) override {}
  void log(LogLevel, etl::string_view, std::int32_t) override {}
  void log(LogLevel, etl::string_view, std::uint32_t) override {}
  void log(LogLevel, etl::string_view, float) override {}
  void set_level(LogLevel) override {}
  LogLevel get_level() const override { 
    return LogLevel::NONE; 
  }
};

}
#endif

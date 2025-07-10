#include "null_logger.h"

namespace musin {

void NullLogger::log(LogLevel, etl::string_view) {
}
void NullLogger::log(LogLevel, etl::string_view, std::int32_t) {
}
void NullLogger::log(LogLevel, etl::string_view, std::uint32_t) {
}
void NullLogger::log(LogLevel, etl::string_view, float) {
}
void NullLogger::set_level(LogLevel) {
}
LogLevel NullLogger::get_level() const {
  return LogLevel::NONE;
}

} // namespace musin
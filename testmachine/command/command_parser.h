#ifndef TESTMACHINE_COMMAND_COMMAND_PARSER_H
#define TESTMACHINE_COMMAND_COMMAND_PARSER_H

#include "etl/string.h"
#include "etl/string_view.h"
#include "etl/vector.h"
#include <cstdint>
#include <cstdlib>

namespace testmachine {

struct Command {
  static constexpr size_t MAX_NAME_LENGTH = 32;
  static constexpr size_t MAX_ARGS = 4;
  static constexpr size_t MAX_ARG_LENGTH = 16;

  etl::string<MAX_NAME_LENGTH> name;
  etl::vector<etl::string<MAX_ARG_LENGTH>, MAX_ARGS> args;

  [[nodiscard]] uint32_t get_arg_uint(size_t index, uint32_t default_value) const {
    if (index >= args.size()) {
      return default_value;
    }
    char *end = nullptr;
    unsigned long val = strtoul(args[index].c_str(), &end, 10);
    if (end == args[index].c_str()) {
      return default_value;
    }
    return static_cast<uint32_t>(val);
  }
};

class CommandParser {
public:
  static constexpr size_t MAX_LINE_LENGTH = 128;

  CommandParser() = default;

  void update();

  [[nodiscard]] bool has_command() const;

  [[nodiscard]] Command get_command();

private:
  void parse_line();

  etl::string<MAX_LINE_LENGTH> line_buffer_;
  Command pending_command_;
  bool command_ready_ = false;
};

} // namespace testmachine

#endif // TESTMACHINE_COMMAND_COMMAND_PARSER_H

#include "command_parser.h"

extern "C" {
#include "pico/stdio.h"
}

#include <cctype>

namespace testmachine {

void CommandParser::update() {
  if (command_ready_) {
    return;
  }

  int ch = getchar_timeout_us(0);
  while (ch != PICO_ERROR_TIMEOUT) {
    if (ch == '\n' || ch == '\r') {
      if (!line_buffer_.empty()) {
        parse_line();
        line_buffer_.clear();
      }
    } else if (line_buffer_.size() < MAX_LINE_LENGTH - 1) {
      line_buffer_.push_back(static_cast<char>(ch));
    }
    ch = getchar_timeout_us(0);
  }
}

bool CommandParser::has_command() const { return command_ready_; }

Command CommandParser::get_command() {
  command_ready_ = false;
  return pending_command_;
}

void CommandParser::parse_line() {
  pending_command_.name.clear();
  pending_command_.args.clear();

  size_t pos = 0;
  const size_t len = line_buffer_.size();

  while (pos < len && std::isspace(static_cast<unsigned char>(line_buffer_[pos]))) {
    ++pos;
  }

  while (pos < len && !std::isspace(static_cast<unsigned char>(line_buffer_[pos]))) {
    if (pending_command_.name.size() < Command::MAX_NAME_LENGTH - 1) {
      pending_command_.name.push_back(
          static_cast<char>(std::toupper(static_cast<unsigned char>(line_buffer_[pos]))));
    }
    ++pos;
  }

  while (pos < len && pending_command_.args.size() < Command::MAX_ARGS) {
    while (pos < len && std::isspace(static_cast<unsigned char>(line_buffer_[pos]))) {
      ++pos;
    }

    if (pos >= len) {
      break;
    }

    etl::string<Command::MAX_ARG_LENGTH> arg;
    while (pos < len && !std::isspace(static_cast<unsigned char>(line_buffer_[pos]))) {
      if (arg.size() < Command::MAX_ARG_LENGTH - 1) {
        arg.push_back(line_buffer_[pos]);
      }
      ++pos;
    }

    if (!arg.empty()) {
      pending_command_.args.push_back(arg);
    }
  }

  if (!pending_command_.name.empty()) {
    command_ready_ = true;
  }
}

} // namespace testmachine

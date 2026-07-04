#pragma once

#include <string>

namespace prisminfer {

enum class ExitCode {
  Ok = 0,
  Usage = 1,
  FailedClosed = 2,
  IoError = 3,
};

std::string to_string(ExitCode code);

}  // namespace prisminfer


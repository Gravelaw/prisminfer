#include "prisminfer/errors.h"

namespace prisminfer {

std::string to_string(ExitCode code) {
  switch (code) {
    case ExitCode::Ok:
      return "ok";
    case ExitCode::Usage:
      return "usage";
    case ExitCode::FailedClosed:
      return "failed_closed";
    case ExitCode::IoError:
      return "io_error";
  }
  return "unknown";
}

}  // namespace prisminfer


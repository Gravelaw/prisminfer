#include <iostream>
#include <string>

#include "prisminfer/benchmark.h"
#include "prisminfer/errors.h"
#include "prisminfer/telemetry.h"

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "Usage: prism-validate-lifecycle TELEMETRY_JSONL\n";
    return static_cast<int>(prisminfer::ExitCode::Usage);
  }

  std::string error;
  const bool valid = prisminfer::validate_phase0_lifecycle(argv[1], &error);
  std::cout << "{\"valid\":" << (valid ? "true" : "false")
            << ",\"path\":\"" << prisminfer::json_escape(argv[1])
            << "\",\"error\":\"" << prisminfer::json_escape(error)
            << "\"}\n";

  return valid ? static_cast<int>(prisminfer::ExitCode::Ok)
               : static_cast<int>(prisminfer::ExitCode::FailedClosed);
}

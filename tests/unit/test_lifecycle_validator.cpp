#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "prisminfer/benchmark.h"

namespace {

int expect(bool condition, const char* message) {
  if (condition) {
    return 0;
  }
  std::cerr << "FAIL: " << message << "\n";
  return 1;
}

}  // namespace

int main() {
  const auto path =
      std::filesystem::temp_directory_path() / "prisminfer_lifecycle.jsonl";
  {
    std::ofstream out(path, std::ios::out | std::ios::trunc);
    out << "{\"event\":\"run_start\"}\n"
        << "{\"event\":\"config_validated\"}\n"
        << "{\"event\":\"telemetry_probe\"}\n"
        << "{\"event\":\"cap_semantics_resolved\"}\n"
        << "{\"event\":\"memory_sample\"}\n"
        << "{\"event\":\"cap_certification_result\"}\n"
        << "{\"event\":\"completed\"}\n"
        << "{\"event\":\"run_end\"}\n";
  }

  std::string error;
  if (expect(prisminfer::validate_phase0_lifecycle(path, &error),
             error.c_str())) return 1;

  {
    std::ofstream out(path, std::ios::out | std::ios::trunc);
    out << "{\"event\":\"run_start\"}\n"
        << "{\"event\":\"config_validated\"}\n"
        << "{\"event\":\"telemetry_probe\"}\n"
        << "{\"event\":\"completed\"}\n"
        << "{\"event\":\"run_end\"}\n";
  }

  error.clear();
  if (expect(!prisminfer::validate_phase0_lifecycle(path, &error),
             "invalid lifecycle rejected")) return 1;
  if (expect(error == "too_few_lifecycle_events" ||
                 error == "missing_memory_sample",
             "invalid lifecycle reason set")) return 1;

  std::error_code remove_error;
  std::filesystem::remove(path, remove_error);
  return 0;
}


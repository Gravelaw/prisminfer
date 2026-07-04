#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "prisminfer/config.h"
#include "prisminfer/runtime_state.h"
#include "prisminfer/telemetry.h"

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
      std::filesystem::temp_directory_path() / "prisminfer_telemetry_test.jsonl";
  std::error_code remove_error;
  std::filesystem::remove(path, remove_error);

  prisminfer::RuntimeConfig config;
  config.run_id = "test-run-id";
  {
    prisminfer::JsonlTelemetry telemetry(path);
    if (expect(telemetry.ok(), telemetry.error().c_str())) return 1;
    telemetry.emit(prisminfer::event::RunStart, config);
    telemetry.emit_memory_sample(config, prisminfer::MemorySample{}, true);
  }

  std::ifstream in(path);
  if (expect(in.good(), "telemetry file opens for read")) return 1;
  std::stringstream buffer;
  buffer << in.rdbuf();
  in.close();

  const std::string content = buffer.str();
  if (expect(content.find("\"run_id\":\"test-run-id\"") != std::string::npos,
             "run_id written")) return 1;
  if (expect(content.find("\"event\":\"run_start\"") != std::string::npos,
             "run_start event written")) return 1;
  if (expect(content.find("\"event\":\"memory_sample\"") != std::string::npos,
             "memory_sample event written")) return 1;
  if (expect(content.find("\"hard_cap_bytes\":1073741824") !=
                 std::string::npos,
             "hard cap written")) return 1;
  if (expect(content.find("\"allocator_bytes\":0") != std::string::npos,
             "allocator bytes written")) return 1;
  if (expect(content.find("\"device_delta_bytes\":0") != std::string::npos,
             "device delta written")) return 1;

  remove_error.clear();
  std::filesystem::remove(path, remove_error);
  return 0;
}

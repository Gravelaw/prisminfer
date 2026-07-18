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
  config.worker_evidence_available = true;
  config.worker_executable_sha256 = "worker-sha256";
  config.worker_approval_identity = "approval-1";
  config.worker_root_process_id = 1234;
  config.worker_job_identity = "job:1:1234:99";
  config.worker_job_total_processes = 1;
  config.worker_job_peak_active_processes = 1;
  config.worker_root_peak_working_set_bytes = 3072;
  config.worker_root_peak_private_commit_bytes = 2048;
  config.worker_tree_peak_working_set_bytes = 3072;
  config.worker_tree_peak_private_commit_bytes = 4096;
  config.worker_tree_sample_interval_milliseconds = 10;
  config.worker_output_limit_bytes = 1024;
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
  if (expect(content.find("\"schema_version\":\"0.6\"") !=
                 std::string::npos,
             "telemetry schema version written")) return 1;
  if (expect(content.find("\"run_id\":\"test-run-id\"") != std::string::npos,
             "run_id written")) return 1;
  if (expect(content.find("\"worker_executable_sha256\":\"worker-sha256\"") !=
                 std::string::npos &&
                 content.find("\"worker_job_identity\":\"job:1:1234:99\"") !=
                     std::string::npos &&
                 content.find(
                     "\"worker_tree_peak_private_commit_bytes\":4096") !=
                     std::string::npos &&
                 content.find("\"worker_output_limit_bytes\":1024") !=
                     std::string::npos,
             "worker identity and Job evidence written")) return 1;
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

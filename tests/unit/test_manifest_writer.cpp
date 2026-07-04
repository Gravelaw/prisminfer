#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
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
      std::filesystem::temp_directory_path() / "prisminfer_manifest_test.json";
  std::error_code remove_error;
  std::filesystem::remove(path, remove_error);

  prisminfer::RuntimeConfig config;
  config.run_id = "manifest-run";
  config.telemetry_path = "manifest-run.jsonl";
  config.model_parameter_bucket = "<=2B";
  config.parameter_count = 123456;
  config.vram_tier_gib = 1;
  config.validation_cell_id = "cell-manifest";
  config.validation_cell_status = "metadata-only";

  prisminfer::MemorySample sample;
  sample.allocator_peak_bytes = 123;
  sample.process_gpu_peak_bytes = 123;
  sample.device_delta_bytes = 12;
  sample.warmup_peak_bytes = 99;
  sample.unknown_post_warmup_bytes = 7;

  prisminfer::CudaProbeResult cuda;
  cuda.gpu_name = "test gpu";
  cuda.driver_version = 13030;
  cuda.runtime_version = 13030;

  prisminfer::HostTelemetrySample host;
  host.available = true;
  host.process_working_set_bytes = 111;
  host.process_private_bytes = 222;
  host.process_io_read_bytes = 333;
  host.process_io_write_bytes = 444;

  std::string error;
  const prisminfer::ManifestInputs inputs{
      config, sample, host, cuda, "ok", ""};
  if (expect(prisminfer::write_probe_manifest(path, inputs, &error),
             error.c_str())) return 1;

  std::ifstream in(path);
  if (expect(in.good(), "manifest file opens")) return 1;
  std::stringstream buffer;
  buffer << in.rdbuf();
  in.close();
  const auto content = buffer.str();

  if (expect(content.find("\"manifest_version\": \"0.2\"") !=
                 std::string::npos,
             "manifest version written")) return 1;
  if (expect(content.find("\"run_id\": \"manifest-run\"") !=
                 std::string::npos,
             "run id written")) return 1;
  if (expect(content.find("\"allocator_peak_bytes\": 123") !=
                 std::string::npos,
             "allocator peak written")) return 1;
  if (expect(content.find("\"model_parameter_bucket\": \"<=2B\"") !=
                 std::string::npos,
             "model parameter bucket written")) return 1;
  if (expect(content.find("\"parameter_count\": 123456") !=
                 std::string::npos,
             "parameter count written")) return 1;
  if (expect(content.find("\"validation_cell_id\": \"cell-manifest\"") !=
                 std::string::npos,
             "validation cell id written")) return 1;
  if (expect(content.find("\"backend\": \"null\"") != std::string::npos,
             "backend written")) return 1;
  if (expect(content.find("\"backend_adapter_contract_version\": \"0.2\"") !=
                 std::string::npos,
             "backend contract version written")) return 1;
  if (expect(content.find("\"host_telemetry_available\": true") !=
                 std::string::npos,
             "host telemetry availability written")) return 1;
  if (expect(content.find("\"process_working_set_bytes\": 111") !=
                 std::string::npos,
             "host working set written")) return 1;
  if (expect(content.find("\"build_config\":") != std::string::npos,
             "build config written")) return 1;
  if (expect(content.find("\"warmup_peak_bytes\": 99") !=
                 std::string::npos,
             "warmup peak written")) return 1;
  if (expect(content.find("\"unknown_post_warmup_bytes\": 7") !=
                 std::string::npos,
             "unknown post-warmup bytes written")) return 1;
  if (expect(content.find("\"gpu_name\": \"test gpu\"") !=
                 std::string::npos,
             "gpu name written")) return 1;

  remove_error.clear();
  std::filesystem::remove(path, remove_error);
  return 0;
}

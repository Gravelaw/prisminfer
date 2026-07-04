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
        << "{\"event\":\"backend_selected\"}\n"
        << "{\"event\":\"dependency_pins_resolved\"}\n"
        << "{\"event\":\"cap_semantics_resolved\"}\n"
        << "{\"event\":\"host_prepare\"}\n"
        << "{\"event\":\"model_load_plan\"}\n"
        << "{\"event\":\"backend_init\"}\n"
        << "{\"event\":\"backend_warmup\"}\n"
        << "{\"event\":\"kv_cache_profile\"}\n"
        << "{\"event\":\"prefill_start\"}\n"
        << "{\"event\":\"kv_cache_sample\"}\n"
        << "{\"event\":\"prefill_end\"}\n"
        << "{\"event\":\"decode_start\"}\n"
        << "{\"event\":\"decode_token\"}\n"
        << "{\"event\":\"decode_end\"}\n"
        << "{\"event\":\"quality_gate_result\"}\n"
        << "{\"event\":\"baseline_selected\"}\n"
        << "{\"event\":\"transfer_plan\"}\n"
        << "{\"event\":\"transfer_sample\"}\n"
        << "{\"event\":\"offload_sample\"}\n"
        << "{\"event\":\"profitability_result\"}\n"
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

  {
    std::ofstream out(path, std::ios::out | std::ios::trunc);
    out << "{\"event\":\"run_start\"}\n"
        << "{\"event\":\"config_validated\"}\n"
        << "{\"event\":\"telemetry_probe\"}\n"
        << "{\"event\":\"cuda_context_probe\"}\n"
        << "{\"event\":\"memory_sample\"}\n"
        << "{\"event\":\"failed_closed\"}\n"
        << "{\"event\":\"run_end\"}\n";
  }

  error.clear();
  if (expect(prisminfer::validate_phase0_lifecycle(path, &error),
             "early CUDA fail-closed lifecycle accepted")) {
    return 1;
  }

  {
    std::ofstream out(path, std::ios::out | std::ios::trunc);
    out << "{\"event\":\"run_start\"}\n"
        << "{\"event\":\"config_validated\"}\n"
        << "{\"event\":\"telemetry_probe\"}\n"
        << "{\"event\":\"cap_semantics_resolved\"}\n"
        << "{\"event\":\"host_prepare\"}\n"
        << "{\"event\":\"model_load_plan\"}\n"
        << "{\"event\":\"backend_init\"}\n"
        << "{\"event\":\"memory_sample\"}\n"
        << "{\"event\":\"cap_certification_result\"}\n"
        << "{\"event\":\"completed\"}\n"
        << "{\"event\":\"run_end\"}\n";
  }

  error.clear();
  if (expect(!prisminfer::validate_phase0_lifecycle(path, &error),
             "missing backend selected rejected")) return 1;
  if (expect(error == "missing_backend_selected",
             "missing backend selected reason set")) return 1;

  {
    std::ofstream out(path, std::ios::out | std::ios::trunc);
    out << "{\"event\":\"run_start\"}\n"
        << "{\"event\":\"config_validated\"}\n"
        << "{\"event\":\"telemetry_probe\"}\n"
        << "{\"event\":\"backend_selected\"}\n"
        << "{\"event\":\"dependency_pins_resolved\"}\n"
        << "{\"event\":\"cap_semantics_resolved\"}\n"
        << "{\"event\":\"host_prepare\"}\n"
        << "{\"event\":\"model_load_plan\"}\n"
        << "{\"event\":\"backend_init\"}\n"
        << "{\"event\":\"memory_sample\"}\n"
        << "{\"event\":\"cap_certification_result\"}\n"
        << "{\"event\":\"completed\"}\n"
        << "{\"event\":\"run_end\"}\n";
  }

  error.clear();
  if (expect(!prisminfer::validate_phase0_lifecycle(path, &error),
             "missing backend warmup rejected")) return 1;
  if (expect(error == "missing_backend_warmup",
             "missing backend warmup reason set")) return 1;

  {
    std::ofstream out(path, std::ios::out | std::ios::trunc);
    out << "{\"event\":\"run_start\"}\n"
        << "{\"event\":\"config_validated\"}\n"
        << "{\"event\":\"telemetry_probe\"}\n"
        << "{\"event\":\"backend_selected\"}\n"
        << "{\"event\":\"dependency_pins_resolved\"}\n"
        << "{\"event\":\"cap_semantics_resolved\"}\n"
        << "{\"event\":\"host_prepare\"}\n"
        << "{\"event\":\"model_load_plan\"}\n"
        << "{\"event\":\"backend_init\"}\n"
        << "{\"event\":\"backend_warmup\"}\n"
        << "{\"event\":\"baseline_selected\"}\n"
        << "{\"event\":\"transfer_plan\"}\n"
        << "{\"event\":\"memory_sample\"}\n"
        << "{\"event\":\"cap_certification_result\"}\n"
        << "{\"event\":\"failed_closed\"}\n"
        << "{\"event\":\"run_end\"}\n";
  }

  error.clear();
  if (expect(!prisminfer::validate_phase0_lifecycle(path, &error),
             "missing phase3 lifecycle rejected")) return 1;
  if (expect(error == "missing_phase3_profitability_lifecycle_event",
             "missing phase3 lifecycle reason set")) return 1;

  {
    std::ofstream out(path, std::ios::out | std::ios::trunc);
    out << "{\"event\":\"run_start\"}\n"
        << "{\"event\":\"config_validated\"}\n"
        << "{\"event\":\"telemetry_probe\"}\n"
        << "{\"event\":\"model_sidecar_validated\"}\n"
        << "{\"event\":\"memory_sample\"}\n"
        << "{\"event\":\"failed_closed\"}\n"
        << "{\"event\":\"run_end\"}\n";
  }

  error.clear();
  if (expect(prisminfer::validate_phase0_lifecycle(path, &error),
             "early sidecar fail-closed lifecycle accepted")) {
    return 1;
  }

  std::error_code remove_error;
  std::filesystem::remove(path, remove_error);
  return 0;
}

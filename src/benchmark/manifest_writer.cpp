#include "prisminfer/benchmark.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>

#include "prisminfer/backend.h"
#include "prisminfer/telemetry.h"

namespace prisminfer {

namespace {

std::string compiler_name() {
#if defined(_MSC_VER)
  return "MSVC " + std::to_string(_MSC_VER);
#elif defined(__clang__)
  return "Clang " + std::string(__clang_version__);
#elif defined(__GNUC__)
  return "GCC " + std::to_string(__GNUC__);
#else
  return "unknown";
#endif
}

std::string os_name() {
#if defined(_WIN32)
  return "windows";
#elif defined(__linux__)
  return "linux";
#else
  return "unknown";
#endif
}

bool line_contains(const std::string& line, const std::string& needle) {
  return line.find(needle) != std::string::npos;
}

}  // namespace

bool write_probe_manifest(const std::filesystem::path& path,
                          const ManifestInputs& inputs,
                          std::string* error) {
  std::ofstream out(path, std::ios::out | std::ios::trunc);
  if (!out) {
    if (error != nullptr) {
      *error = "failed_to_open_manifest_path: " + path.string();
    }
    return false;
  }

  out << "{\n"
      << "  \"manifest_version\": \"0.2\",\n"
      << "  \"run_id\": \"" << json_escape(inputs.config.run_id) << "\",\n"
      << "  \"created_at_utc\": \"" << utc_timestamp() << "\",\n"
      << "  \"tool\": \"prism-probe\",\n"
      << "  \"prisminfer_version\": \"" << PRISMINFER_VERSION << "\",\n"
      << "  \"cmake_version\": \"" << PRISMINFER_CMAKE_VERSION << "\",\n"
      << "  \"build_config\": \"" << PRISMINFER_BUILD_CONFIG << "\",\n"
      << "  \"compiler\": \"" << json_escape(compiler_name()) << "\",\n"
      << "  \"os\": \"" << os_name() << "\",\n"
      << "  \"cuda_probe_compiled\": "
#if defined(PRISMINFER_ENABLE_CUDA_PROBE)
      << "true,\n"
#else
      << "false,\n"
#endif
      << "  \"mode\": \"" << json_escape(to_string(inputs.config.mode))
      << "\",\n"
      << "  \"hard_cap_bytes\": " << inputs.config.hard_cap_bytes << ",\n"
      << "  \"model_parameter_bucket\": \""
      << json_escape(inputs.config.model_parameter_bucket) << "\",\n"
      << "  \"parameter_count\": " << inputs.config.parameter_count << ",\n"
      << "  \"vram_tier_gib\": " << inputs.config.vram_tier_gib << ",\n"
      << "  \"validation_cell_id\": \""
      << json_escape(inputs.config.validation_cell_id) << "\",\n"
      << "  \"validation_cell_status\": \""
      << json_escape(inputs.config.validation_cell_status) << "\",\n"
      << "  \"backend\": \"" << json_escape(to_string(inputs.config.backend))
      << "\",\n"
      << "  \"backend_adapter_contract_version\": \""
      << kBackendContractVersion << "\",\n"
      << "  \"backend_required\": "
      << (inputs.config.backend_required ? "true" : "false") << ",\n"
      << "  \"dependency_pin_file\": \""
      << json_escape(inputs.config.dependency_pin_file.generic_string())
      << "\",\n"
      << "  \"llama_executable\": \""
      << json_escape(inputs.config.llama_executable_path.generic_string())
      << "\",\n"
      << "  \"context_tokens\": " << inputs.config.context_tokens << ",\n"
      << "  \"gpu_layers\": " << inputs.config.gpu_layers << ",\n"
      << "  \"mmap_enabled\": "
      << (inputs.config.mmap_enabled ? "true" : "false") << ",\n"
      << "  \"warmup_tokens\": " << inputs.config.warmup_tokens << ",\n"
      << "  \"telemetry_path\": \""
      << json_escape(inputs.config.telemetry_path) << "\",\n"
      << "  \"status\": \"" << json_escape(inputs.status) << "\",\n"
      << "  \"failure_reason\": \"" << json_escape(inputs.failure_reason)
      << "\",\n"
      << "  \"gpu_name\": \"" << json_escape(inputs.cuda_probe.gpu_name)
      << "\",\n"
      << "  \"cuda_driver_version\": " << inputs.cuda_probe.driver_version
      << ",\n"
      << "  \"cuda_runtime_version\": " << inputs.cuda_probe.runtime_version
      << ",\n"
      << "  \"device_total_bytes\": " << inputs.cuda_probe.device_total_bytes
      << ",\n"
      << "  \"device_used_bytes\": " << inputs.sample.device_used_bytes
      << ",\n"
      << "  \"device_delta_bytes\": " << inputs.sample.device_delta_bytes
      << ",\n"
      << "  \"allocator_peak_bytes\": "
      << inputs.sample.allocator_peak_bytes << ",\n"
      << "  \"process_gpu_peak_bytes\": "
      << inputs.sample.process_gpu_peak_bytes << ",\n"
      << "  \"warmup_peak_bytes\": " << inputs.sample.warmup_peak_bytes
      << ",\n"
      << "  \"retained_pool_bytes\": " << inputs.sample.retained_pool_bytes
      << ",\n"
      << "  \"unknown_post_warmup_bytes\": "
      << inputs.sample.unknown_post_warmup_bytes << ",\n"
      << "  \"host_telemetry_available\": "
      << (inputs.host.available ? "true" : "false") << ",\n"
      << "  \"host_telemetry_unavailable_reason\": \""
      << json_escape(inputs.host.unavailable_reason) << "\",\n"
      << "  \"process_working_set_bytes\": "
      << inputs.host.process_working_set_bytes << ",\n"
      << "  \"process_private_bytes\": "
      << inputs.host.process_private_bytes << ",\n"
      << "  \"system_memory_available_bytes\": "
      << inputs.host.system_memory_available_bytes << ",\n"
      << "  \"system_commit_total_bytes\": "
      << inputs.host.system_commit_total_bytes << ",\n"
      << "  \"system_commit_limit_bytes\": "
      << inputs.host.system_commit_limit_bytes << ",\n"
      << "  \"process_io_read_bytes\": "
      << inputs.host.process_io_read_bytes << ",\n"
      << "  \"process_io_write_bytes\": "
      << inputs.host.process_io_write_bytes << ",\n"
      << "  \"required_telemetry_present\": "
      << (inputs.sample.required_telemetry_present ? "true" : "false")
      << ",\n"
      << "  \"telemetry_agreement\": "
      << (inputs.sample.telemetry_agreement ? "true" : "false") << "\n"
      << "}\n";

  return true;
}

bool validate_phase0_lifecycle(const std::filesystem::path& telemetry_path,
                               std::string* error) {
  std::ifstream in(telemetry_path);
  if (!in) {
    if (error != nullptr) {
      *error = "failed_to_open_telemetry_path: " + telemetry_path.string();
    }
    return false;
  }

  std::vector<std::string> events;
  std::string line;
  while (std::getline(in, line)) {
    if (line_contains(line, "\"event\":\"run_start\"")) {
      events.emplace_back("run_start");
    } else if (line_contains(line, "\"event\":\"config_validated\"")) {
      events.emplace_back("config_validated");
    } else if (line_contains(line, "\"event\":\"telemetry_probe\"")) {
      events.emplace_back("telemetry_probe");
    } else if (line_contains(line, "\"event\":\"model_sidecar_validated\"")) {
      events.emplace_back("model_sidecar_validated");
    } else if (line_contains(line, "\"event\":\"backend_selected\"")) {
      events.emplace_back("backend_selected");
    } else if (line_contains(line, "\"event\":\"dependency_pins_resolved\"")) {
      events.emplace_back("dependency_pins_resolved");
    } else if (line_contains(line, "\"event\":\"cuda_context_probe\"")) {
      events.emplace_back("cuda_context_probe");
    } else if (line_contains(line, "\"event\":\"cap_semantics_resolved\"")) {
      events.emplace_back("cap_semantics_resolved");
    } else if (line_contains(line, "\"event\":\"host_prepare\"")) {
      events.emplace_back("host_prepare");
    } else if (line_contains(line, "\"event\":\"model_load_plan\"")) {
      events.emplace_back("model_load_plan");
    } else if (line_contains(line, "\"event\":\"backend_init\"")) {
      events.emplace_back("backend_init");
    } else if (line_contains(line, "\"event\":\"backend_warmup\"")) {
      events.emplace_back("backend_warmup");
    } else if (line_contains(line, "\"event\":\"memory_sample\"")) {
      events.emplace_back("memory_sample");
    } else if (line_contains(line, "\"event\":\"cap_certification_result\"")) {
      events.emplace_back("cap_certification_result");
    } else if (line_contains(line, "\"event\":\"completed\"")) {
      events.emplace_back("completed");
    } else if (line_contains(line, "\"event\":\"failed_closed\"")) {
      events.emplace_back("failed_closed");
    } else if (line_contains(line, "\"event\":\"run_end\"")) {
      events.emplace_back("run_end");
    }
  }

  const auto fail = [&](const std::string& message) {
    if (error != nullptr) {
      *error = message;
    }
    return false;
  };

  if (events.size() < 6) {
    return fail("too_few_lifecycle_events");
  }
  if (events.front() != "run_start") {
    return fail("first_event_not_run_start");
  }
  if (events.back() != "run_end") {
    return fail("last_event_not_run_end");
  }

  std::size_t position = 0;
  const auto require_next = [&](const std::string& expected) {
    if (position >= events.size() || events[position] != expected) {
      return false;
    }
    ++position;
    return true;
  };

  if (!require_next("run_start") || !require_next("config_validated") ||
      !require_next("telemetry_probe")) {
    return fail("required_prefix_missing");
  }
  if (position < events.size() && events[position] == "cuda_context_probe") {
    ++position;
  }
  if (position < events.size() &&
      events[position] == "model_sidecar_validated") {
    ++position;
  }
  if (position < events.size() && events[position] == "cuda_context_probe") {
    ++position;
  }

  bool saw_terminal = false;
  bool saw_memory = false;
  bool saw_cap_semantics = false;
  bool saw_host_prepare = false;
  bool saw_backend_selected = false;
  bool saw_dependency_pins = false;
  bool saw_model_load_plan = false;
  bool saw_backend_init = false;
  bool saw_backend_warmup = false;
  for (; position < events.size(); ++position) {
    if (events[position] == "cap_semantics_resolved") {
      saw_cap_semantics = true;
    }
    if (events[position] == "host_prepare") {
      saw_host_prepare = true;
    }
    if (events[position] == "backend_selected") {
      saw_backend_selected = true;
    }
    if (events[position] == "dependency_pins_resolved") {
      saw_dependency_pins = true;
    }
    if (events[position] == "model_load_plan") {
      saw_model_load_plan = true;
    }
    if (events[position] == "backend_init") {
      saw_backend_init = true;
    }
    if (events[position] == "backend_warmup") {
      saw_backend_warmup = true;
    }
    if (events[position] == "memory_sample") {
      saw_memory = true;
    }
    if (events[position] == "completed" || events[position] == "failed_closed") {
      saw_terminal = true;
    }
  }

  if (!saw_memory) {
    return fail("missing_memory_sample");
  }
  const bool early_cuda_fail_closed =
      saw_terminal && !saw_cap_semantics &&
      std::find(events.begin(), events.end(), "cuda_context_probe") !=
          events.end();
  const bool early_sidecar_fail_closed =
      saw_terminal && !saw_cap_semantics &&
      std::find(events.begin(), events.end(), "model_sidecar_validated") !=
          events.end();
  if (!saw_cap_semantics && !early_cuda_fail_closed &&
      !early_sidecar_fail_closed) {
    return fail("missing_cap_semantics_resolved");
  }
  if (!saw_host_prepare && !early_cuda_fail_closed &&
      !early_sidecar_fail_closed) {
    return fail("missing_host_prepare");
  }
  if (!saw_backend_selected && !early_cuda_fail_closed &&
      !early_sidecar_fail_closed) {
    return fail("missing_backend_selected");
  }
  if (!saw_dependency_pins && !early_cuda_fail_closed &&
      !early_sidecar_fail_closed) {
    return fail("missing_dependency_pins_resolved");
  }
  if (!saw_model_load_plan && !early_cuda_fail_closed &&
      !early_sidecar_fail_closed) {
    return fail("missing_model_load_plan");
  }
  if (!saw_backend_init && !early_cuda_fail_closed &&
      !early_sidecar_fail_closed) {
    return fail("missing_backend_init");
  }
  if (!saw_backend_warmup && !early_cuda_fail_closed &&
      !early_sidecar_fail_closed) {
    return fail("missing_backend_warmup");
  }
  if (!saw_terminal) {
    return fail("missing_terminal_event");
  }
  return true;
}

}  // namespace prisminfer

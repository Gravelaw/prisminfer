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
      << "  \"manifest_version\": \"0.5\",\n"
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
      << "  \"kernel_build_enabled\": "
#if defined(PRISMINFER_ENABLE_CUDA_KERNELS)
      << "true,\n"
#else
      << "false,\n"
#endif
      << "  \"kernel_cuda_archs\": \""
      << json_escape(PRISMINFER_CUDA_KERNEL_ARCHS) << "\",\n"
      << "  \"cublaslt_baseline_available\": "
#if defined(PRISMINFER_ENABLE_CUBLASLT_BASELINE)
      << "true,\n"
#else
      << "false,\n"
#endif
      << "  \"cutlass_baseline_available\": "
#if defined(PRISMINFER_ENABLE_CUTLASS_BASELINE)
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
      << "  \"kv_accounting\": "
      << (inputs.config.kv_accounting ? "true" : "false") << ",\n"
      << "  \"kv_layer_count\": " << inputs.kv_profile.layer_count << ",\n"
      << "  \"kv_head_count\": " << inputs.kv_profile.kv_head_count << ",\n"
      << "  \"kv_head_dim\": " << inputs.kv_profile.head_dim << ",\n"
      << "  \"kv_block_tokens\": " << inputs.kv_profile.block_tokens << ",\n"
      << "  \"kv_key_bits\": " << inputs.kv_profile.key_bits << ",\n"
      << "  \"kv_value_bits\": " << inputs.kv_profile.value_bits << ",\n"
      << "  \"kv_bytes_per_token\": " << inputs.kv_profile.bytes_per_token
      << ",\n"
      << "  \"kv_bytes_per_block\": " << inputs.kv_profile.bytes_per_block
      << ",\n"
      << "  \"kv_placement\": \"" << json_escape(inputs.config.kv_placement)
      << "\",\n"
      << "  \"kv_compression\": \""
      << json_escape(inputs.config.kv_compression) << "\",\n"
      << "  \"kv_logical_tokens\": " << inputs.kv_sample.logical_tokens
      << ",\n"
      << "  \"kv_active_blocks\": " << inputs.kv_sample.active_blocks
      << ",\n"
      << "  \"kv_gpu_bytes\": " << inputs.kv_sample.gpu_bytes << ",\n"
      << "  \"kv_host_bytes\": " << inputs.kv_sample.host_bytes << ",\n"
      << "  \"kv_compressed_bytes\": " << inputs.kv_sample.compressed_bytes
      << ",\n"
      << "  \"kv_metadata_bytes\": " << inputs.kv_sample.metadata_bytes
      << ",\n"
      << "  \"kv_unknown_bytes\": " << inputs.kv_sample.unknown_bytes
      << ",\n"
      << "  \"kv_evidence_status\": \""
      << json_escape(inputs.kv_sample.evidence_status) << "\",\n"
      << "  \"kv_metadata_budget_bytes\": "
      << inputs.config.kv_metadata_budget_bytes << ",\n"
      << "  \"quality_gate\": \"" << json_escape(inputs.config.quality_gate)
      << "\",\n"
      << "  \"quality_baseline_manifest\": \""
      << json_escape(inputs.config.quality_baseline_manifest.generic_string())
      << "\",\n"
      << "  \"quality_status\": \"" << json_escape(inputs.quality.status)
      << "\",\n"
      << "  \"quality_passed\": "
      << (inputs.quality.passed ? "true" : "false") << ",\n"
      << "  \"quality_failure_reason\": \""
      << json_escape(inputs.quality.failure_reason) << "\",\n"
      << "  \"quality_observed_delta\": " << inputs.quality.observed_delta
      << ",\n"
      << "  \"kv_compression_status\": \""
      << json_escape(inputs.compression.status) << "\",\n"
      << "  \"kv_compression_accepted\": "
      << (inputs.compression.accepted ? "true" : "false") << ",\n"
      << "  \"kv_compression_failure_reason\": \""
      << json_escape(inputs.compression.failure_reason) << "\",\n"
      << "  \"kv_saved_bytes\": " << inputs.compression.saved_bytes << ",\n"
      << "  \"compression_status\": \""
      << json_escape(inputs.compression.status) << "\",\n"
      << "  \"compression_profile_id\": \"\",\n"
      << "  \"quantization_scope\": \""
      << (inputs.config.kv_compression == "none" ? "none" : "kv-cache")
      << "\",\n"
      << "  \"algorithm_family\": \""
      << json_escape(inputs.config.kv_compression) << "\",\n"
      << "  \"payload_bits_per_value\": 0,\n"
      << "  \"effective_bits_per_value\": 0,\n"
      << "  \"metadata_bits_per_value\": 0,\n"
      << "  \"key_quant_axis\": \"\",\n"
      << "  \"value_quant_axis\": \"\",\n"
      << "  \"pre_rope_or_post_rope\": \"\",\n"
      << "  \"group_size\": 0,\n"
      << "  \"residual_fp_window_tokens\": 0,\n"
      << "  \"outlier_policy\": \"\",\n"
      << "  \"rotation_policy\": \"\",\n"
      << "  \"rotation_seed\": 0,\n"
      << "  \"projection_policy\": \"\",\n"
      << "  \"qjl_residual_bits\": 0,\n"
      << "  \"dequant_workspace_peak_bytes\": 0,\n"
      << "  \"kv_payload_bytes\": " << inputs.kv_sample.compressed_bytes
      << ",\n"
      << "  \"kv_residual_or_sketch_bytes\": 0,\n"
      << "  \"compression_decode_overhead_ms\": 0,\n"
      << "  \"attention_logit_error_p95\": 0,\n"
      << "  \"attention_logit_error_p99\": 0,\n"
      << "  \"attention_topk_overlap\": 0,\n"
      << "  \"quality_gate_id\": \""
      << json_escape(inputs.config.quality_gate) << "\",\n"
      << "  \"quality_result_path\": \""
      << json_escape(inputs.config.quality_baseline_manifest.generic_string())
      << "\",\n"
      << "  \"cap_certification_status\": \"research-only\",\n"
      << "  \"profitability_policy\": \""
      << json_escape(inputs.config.profitability_policy) << "\",\n"
      << "  \"baseline_manifest\": \""
      << json_escape(inputs.config.baseline_manifest.generic_string())
      << "\",\n"
      << "  \"min_speedup_ratio\": " << inputs.config.min_speedup_ratio
      << ",\n"
      << "  \"offload_policy\": \""
      << json_escape(inputs.config.offload_policy) << "\",\n"
      << "  \"pinned_host_budget_bytes\": "
      << inputs.config.pinned_host_budget_bytes << ",\n"
      << "  \"staging_buffer_bytes\": " << inputs.config.staging_buffer_bytes
      << ",\n"
      << "  \"transfer_metrics\": "
      << (inputs.config.transfer_metrics ? "true" : "false") << ",\n"
      << "  \"cold_cache_run\": "
      << (inputs.config.cold_cache_run ? "true" : "false") << ",\n"
      << "  \"cpu_baseline_ttft_ms\": "
      << inputs.config.cpu_baseline_ttft_ms << ",\n"
      << "  \"cpu_baseline_decode_tps\": "
      << inputs.config.cpu_baseline_decode_tps << ",\n"
      << "  \"cpu_baseline_peak_bytes\": "
      << inputs.config.cpu_baseline_peak_bytes << ",\n"
      << "  \"observed_ttft_ms\": "
      << inputs.transfer.time_to_first_token_ms << ",\n"
      << "  \"observed_decode_tps\": "
      << inputs.transfer.decode_tokens_per_second << ",\n"
      << "  \"token_latency_p50_ms\": "
      << inputs.transfer.token_latency_p50_ms << ",\n"
      << "  \"token_latency_p95_ms\": "
      << inputs.transfer.token_latency_p95_ms << ",\n"
      << "  \"h2d_bytes\": " << inputs.transfer.h2d_bytes << ",\n"
      << "  \"d2h_bytes\": " << inputs.transfer.d2h_bytes << ",\n"
      << "  \"nvme_read_bytes\": " << inputs.transfer.nvme_read_bytes
      << ",\n"
      << "  \"nvme_write_bytes\": " << inputs.transfer.nvme_write_bytes
      << ",\n"
      << "  \"pinned_host_peak_bytes\": "
      << inputs.transfer.pinned_host_peak_bytes << ",\n"
      << "  \"staging_peak_bytes\": " << inputs.transfer.staging_peak_bytes
      << ",\n"
      << "  \"h2d_ms\": " << inputs.transfer.h2d_ms << ",\n"
      << "  \"d2h_ms\": " << inputs.transfer.d2h_ms << ",\n"
      << "  \"io_ms\": " << inputs.transfer.io_ms << ",\n"
      << "  \"wait_ms\": " << inputs.transfer.wait_ms << ",\n"
      << "  \"prefill_ms\": " << inputs.transfer.prefill_ms << ",\n"
      << "  \"decode_ms\": " << inputs.transfer.decode_ms << ",\n"
      << "  \"offload_evidence_label\": \""
      << json_escape(inputs.offload_plan.evidence_label) << "\",\n"
      << "  \"profitability_status\": \""
      << json_escape(inputs.profitability.status) << "\",\n"
      << "  \"profitability_accepted\": "
      << (inputs.profitability.accepted ? "true" : "false") << ",\n"
      << "  \"profitability_reason\": \""
      << json_escape(inputs.profitability.reason) << "\",\n"
      << "  \"profitability_speedup_ratio\": "
      << inputs.profitability.speedup_ratio << ",\n"
      << "  \"profitability_required_speedup_ratio\": "
      << inputs.profitability.required_speedup_ratio << ",\n"
      << "  \"claim_label\": \"" << json_escape(inputs.config.claim_label)
      << "\",\n"
      << "  \"quantization_format\": \""
      << json_escape(inputs.config.quantization_format) << "\",\n"
      << "  \"quant_artifact_sha256\": \""
      << json_escape(inputs.config.quant_artifact_sha256) << "\",\n"
      << "  \"host_memory_budget_bytes\": "
      << inputs.config.host_memory_budget_bytes << ",\n"
      << "  \"nvme_budget_bytes\": " << inputs.config.nvme_budget_bytes
      << ",\n"
      << "  \"max_time_to_first_token_ms\": "
      << inputs.config.max_time_to_first_token_ms << ",\n"
      << "  \"min_decode_tokens_per_second\": "
      << inputs.config.min_decode_tokens_per_second << ",\n"
      << "  \"max_token_latency_p95_ms\": "
      << inputs.config.max_token_latency_p95_ms << ",\n"
      << "  \"repeatability_runs\": " << inputs.config.repeatability_runs
      << ",\n"
      << "  \"repeatability_passed\": "
      << (inputs.config.repeatability_passed ? "true" : "false") << ",\n"
      << "  \"claim_validation\": \""
      << json_escape(inputs.config.claim_validation) << "\",\n"
      << "  \"hybrid_plan_status\": \""
      << (inputs.hybrid_plan.ok ? "accepted" : "rejected") << "\",\n"
      << "  \"hybrid_plan_failure_reason\": \""
      << json_escape(inputs.hybrid_plan.failure_reason) << "\",\n"
      << "  \"hybrid_resident_gpu_total_bytes\": "
      << inputs.hybrid_plan.resident_gpu_total_bytes << ",\n"
      << "  \"usability_status\": \"" << json_escape(inputs.usability.status)
      << "\",\n"
      << "  \"usability_reason\": \"" << json_escape(inputs.usability.reason)
      << "\",\n"
      << "  \"claim_validation_status\": \""
      << json_escape(inputs.claim.status) << "\",\n"
      << "  \"claim_validation_accepted\": "
      << (inputs.claim.accepted ? "true" : "false") << ",\n"
      << "  \"claim_validation_reason\": \""
      << json_escape(inputs.claim.reason) << "\",\n"
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
      << "  \"kv_gpu_peak_bytes\": " << inputs.sample.kv_gpu_peak_bytes
      << ",\n"
      << "  \"kv_host_peak_bytes\": " << inputs.sample.kv_host_peak_bytes
      << ",\n"
      << "  \"kv_compressed_peak_bytes\": "
      << inputs.sample.kv_compressed_peak_bytes << ",\n"
      << "  \"kv_metadata_peak_bytes\": "
      << inputs.sample.kv_metadata_peak_bytes << ",\n"
      << "  \"kv_unknown_peak_bytes\": " << inputs.sample.kv_unknown_peak_bytes
      << ",\n"
      << "  \"unknown_post_warmup_bytes\": "
      << inputs.sample.unknown_post_warmup_bytes << ",\n"
      << "  \"host_telemetry_available\": "
      << (inputs.host.available ? "true" : "false") << ",\n"
      << "  \"host_telemetry_unavailable_reason\": \""
      << json_escape(inputs.host.unavailable_reason) << "\",\n"
      << "  \"system_commit_source\": \""
      << json_escape(inputs.host.system_commit_source) << "\",\n"
      << "  \"host_captured_monotonic_milliseconds\": "
      << inputs.host.captured_monotonic_milliseconds << ",\n"
      << "  \"process_working_set_bytes\": "
      << inputs.host.process_working_set_bytes << ",\n"
      << "  \"process_private_bytes\": "
      << inputs.host.process_private_bytes << ",\n"
      << "  \"system_memory_total_bytes\": "
      << inputs.host.system_memory_total_bytes << ",\n"
      << "  \"system_memory_available_bytes\": "
      << inputs.host.system_memory_available_bytes << ",\n"
      << "  \"system_commit_total_bytes\": "
      << inputs.host.system_commit_total_bytes << ",\n"
      << "  \"system_commit_limit_bytes\": "
      << inputs.host.system_commit_limit_bytes << ",\n"
      << "  \"system_commit_available_bytes\": "
      << inputs.host.system_commit_available_bytes << ",\n"
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
    } else if (line_contains(line, "\"event\":\"kv_cache_profile\"")) {
      events.emplace_back("kv_cache_profile");
    } else if (line_contains(line, "\"event\":\"prefill_start\"")) {
      events.emplace_back("prefill_start");
    } else if (line_contains(line, "\"event\":\"prefill_end\"")) {
      events.emplace_back("prefill_end");
    } else if (line_contains(line, "\"event\":\"kv_cache_sample\"")) {
      events.emplace_back("kv_cache_sample");
    } else if (line_contains(line, "\"event\":\"decode_start\"")) {
      events.emplace_back("decode_start");
    } else if (line_contains(line, "\"event\":\"decode_token\"")) {
      events.emplace_back("decode_token");
    } else if (line_contains(line, "\"event\":\"decode_end\"")) {
      events.emplace_back("decode_end");
    } else if (line_contains(line, "\"event\":\"quality_gate_result\"")) {
      events.emplace_back("quality_gate_result");
    } else if (line_contains(line, "\"event\":\"baseline_selected\"")) {
      events.emplace_back("baseline_selected");
    } else if (line_contains(line, "\"event\":\"transfer_plan\"")) {
      events.emplace_back("transfer_plan");
    } else if (line_contains(line, "\"event\":\"transfer_sample\"")) {
      events.emplace_back("transfer_sample");
    } else if (line_contains(line, "\"event\":\"offload_sample\"")) {
      events.emplace_back("offload_sample");
    } else if (line_contains(line, "\"event\":\"profitability_result\"")) {
      events.emplace_back("profitability_result");
    } else if (line_contains(line, "\"event\":\"hybrid_plan_created\"")) {
      events.emplace_back("hybrid_plan_created");
    } else if (line_contains(line, "\"event\":\"claim_classified\"")) {
      events.emplace_back("claim_classified");
    } else if (line_contains(line, "\"event\":\"usability_result\"")) {
      events.emplace_back("usability_result");
    } else if (line_contains(line, "\"event\":\"repeatability_result\"")) {
      events.emplace_back("repeatability_result");
    } else if (line_contains(line, "\"event\":\"claim_validation_result\"")) {
      events.emplace_back("claim_validation_result");
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
  bool saw_kv_profile = false;
  bool saw_prefill_start = false;
  bool saw_kv_sample = false;
  bool saw_prefill_end = false;
  bool saw_decode_start = false;
  bool saw_decode_end = false;
  bool saw_quality_gate = false;
  bool saw_baseline_selected = false;
  bool saw_transfer_plan = false;
  bool saw_transfer_sample = false;
  bool saw_profitability_result = false;
  bool saw_memory_after_quality = false;
  bool saw_quality_before_memory = false;
  bool saw_phase3_before_memory = false;
  bool saw_hybrid_plan = false;
  bool saw_claim_classified = false;
  bool saw_usability_result = false;
  bool saw_repeatability_result = false;
  bool saw_claim_validation_result = false;
  bool saw_phase4_before_memory = false;
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
    if (events[position] == "kv_cache_profile") {
      saw_kv_profile = true;
    }
    if (events[position] == "prefill_start") {
      saw_prefill_start = true;
    }
    if (events[position] == "kv_cache_sample") {
      saw_kv_sample = true;
    }
    if (events[position] == "prefill_end") {
      saw_prefill_end = true;
    }
    if (events[position] == "decode_start") {
      saw_decode_start = true;
    }
    if (events[position] == "decode_end") {
      saw_decode_end = true;
    }
    if (events[position] == "quality_gate_result") {
      saw_quality_gate = true;
      saw_quality_before_memory = !saw_memory;
    }
    if (events[position] == "baseline_selected") {
      saw_baseline_selected = true;
    }
    if (events[position] == "transfer_plan") {
      saw_transfer_plan = true;
    }
    if (events[position] == "transfer_sample") {
      saw_transfer_sample = true;
    }
    if (events[position] == "profitability_result") {
      saw_profitability_result = true;
      saw_phase3_before_memory = !saw_memory;
    }
    if (events[position] == "hybrid_plan_created") {
      saw_hybrid_plan = true;
    }
    if (events[position] == "claim_classified") {
      saw_claim_classified = true;
    }
    if (events[position] == "usability_result") {
      saw_usability_result = true;
    }
    if (events[position] == "repeatability_result") {
      saw_repeatability_result = true;
    }
    if (events[position] == "claim_validation_result") {
      saw_claim_validation_result = true;
      saw_phase4_before_memory = !saw_memory;
    }
    if (events[position] == "memory_sample") {
      if (saw_quality_gate) {
        saw_memory_after_quality = true;
      }
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
  if (saw_kv_profile) {
    if (!saw_prefill_start || !saw_kv_sample || !saw_prefill_end ||
        !saw_decode_start || !saw_decode_end || !saw_quality_gate) {
      return fail("missing_phase2_kv_lifecycle_event");
    }
    if (!saw_quality_before_memory || !saw_memory_after_quality) {
      return fail("phase2_quality_must_precede_memory_sample");
    }
  }
  if (saw_baseline_selected || saw_transfer_plan || saw_transfer_sample ||
      saw_profitability_result) {
    if (!saw_baseline_selected || !saw_transfer_plan || !saw_transfer_sample ||
        !saw_profitability_result) {
      return fail("missing_phase3_profitability_lifecycle_event");
    }
    if (!saw_phase3_before_memory) {
      return fail("phase3_profitability_must_precede_memory_sample");
    }
  }
  if (saw_hybrid_plan || saw_claim_classified || saw_usability_result ||
      saw_repeatability_result || saw_claim_validation_result) {
    if (!saw_hybrid_plan || !saw_claim_classified || !saw_usability_result ||
        !saw_repeatability_result || !saw_claim_validation_result) {
      return fail("missing_phase4_claim_lifecycle_event");
    }
    if (!saw_phase4_before_memory) {
      return fail("phase4_claim_validation_must_precede_memory_sample");
    }
  }
  if (!saw_terminal) {
    return fail("missing_terminal_event");
  }
  return true;
}

}  // namespace prisminfer

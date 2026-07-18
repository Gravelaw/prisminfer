#include "prisminfer/telemetry.h"

#include <sstream>

namespace prisminfer {

namespace {

bool reserved_event_field(const std::string& key) {
  return key == "schema_version" || key == "timestamp_ns" ||
         key == "run_id" || key == "event" || key == "mode" ||
         key == "hard_cap_bytes" || key == "model_parameter_bucket" ||
         key == "parameter_count" || key == "vram_tier_gib" ||
         key == "validation_cell_id" || key == "validation_cell_status" ||
         key == "backend" || key == "backend_required" ||
         key == "dependency_pin_file" || key == "llama_executable" ||
         key == "context_tokens" || key == "gpu_layers" ||
         key == "mmap_enabled" || key == "warmup_tokens" ||
         key == "worker_evidence_available" ||
         key == "worker_executable_sha256" ||
         key == "worker_approval_identity" || key == "worker_exit_code" ||
         key == "worker_timed_out" ||
         key == "worker_root_process_id" || key == "worker_job_identity" ||
         key == "worker_job_total_processes" ||
         key == "worker_job_peak_active_processes" ||
         key == "worker_job_total_terminated_processes" ||
         key == "worker_root_peak_working_set_bytes" ||
         key == "worker_root_peak_private_commit_bytes" ||
         key == "worker_tree_peak_working_set_bytes" ||
         key == "worker_tree_peak_private_commit_bytes" ||
         key == "worker_tree_sample_interval_milliseconds" ||
         key == "worker_read_bytes" || key == "worker_write_bytes" ||
         key == "worker_output_bytes" || key == "worker_output_limit_bytes" ||
         key == "kv_accounting" || key == "kv_placement" ||
         key == "kv_compression" || key == "quality_gate" ||
         key == "profitability_policy" || key == "offload_policy" ||
         key == "claim_label" || key == "claim_validation";
}

}  // namespace

std::string json_escape(const std::string& value) {
  std::ostringstream escaped;
  for (const char c : value) {
    switch (c) {
      case '\\':
        escaped << "\\\\";
        break;
      case '"':
        escaped << "\\\"";
        break;
      case '\n':
        escaped << "\\n";
        break;
      case '\r':
        escaped << "\\r";
        break;
      case '\t':
        escaped << "\\t";
        break;
      default:
        escaped << c;
        break;
    }
  }
  return escaped.str();
}

JsonlTelemetry::JsonlTelemetry(const std::filesystem::path& path) {
  out_.open(path, std::ios::out | std::ios::trunc);
  if (!out_) {
    error_ = "failed_to_open_telemetry_path: " + path.string();
  }
}

bool JsonlTelemetry::ok() const {
  return out_.is_open() && out_.good();
}

const std::string& JsonlTelemetry::error() const {
  return error_;
}

void JsonlTelemetry::emit(
    const std::string& event,
    const RuntimeConfig& config,
    const std::map<std::string, std::string>& fields) {
  if (!ok()) {
    return;
  }

  out_ << "{\"schema_version\":\"0.6\""
       << ",\"timestamp_ns\":" << monotonic_time_ns()
       << ",\"run_id\":\"" << json_escape(config.run_id) << "\""
       << ",\"event\":\"" << json_escape(event) << "\""
       << ",\"mode\":\"" << json_escape(to_string(config.mode)) << "\""
       << ",\"hard_cap_bytes\":" << config.hard_cap_bytes
       << ",\"model_parameter_bucket\":\""
       << json_escape(config.model_parameter_bucket) << "\""
       << ",\"parameter_count\":" << config.parameter_count
       << ",\"vram_tier_gib\":" << config.vram_tier_gib
       << ",\"validation_cell_id\":\""
       << json_escape(config.validation_cell_id) << "\""
       << ",\"validation_cell_status\":\""
       << json_escape(config.validation_cell_status) << "\""
       << ",\"backend\":\"" << json_escape(to_string(config.backend)) << "\""
       << ",\"backend_required\":"
       << (config.backend_required ? "true" : "false")
       << ",\"dependency_pin_file\":\""
       << json_escape(config.dependency_pin_file.generic_string()) << "\""
       << ",\"llama_executable\":\""
       << json_escape(config.llama_executable_path.generic_string()) << "\""
       << ",\"context_tokens\":" << config.context_tokens
       << ",\"gpu_layers\":" << config.gpu_layers
       << ",\"mmap_enabled\":" << (config.mmap_enabled ? "true" : "false")
       << ",\"warmup_tokens\":" << config.warmup_tokens
       << ",\"worker_evidence_available\":"
       << (config.worker_evidence_available ? "true" : "false")
       << ",\"worker_executable_sha256\":\""
       << json_escape(config.worker_executable_sha256) << "\""
       << ",\"worker_approval_identity\":\""
       << json_escape(config.worker_approval_identity) << "\""
       << ",\"worker_exit_code\":" << config.worker_exit_code
       << ",\"worker_timed_out\":"
       << (config.worker_timed_out ? "true" : "false")
       << ",\"worker_root_process_id\":" << config.worker_root_process_id
       << ",\"worker_job_identity\":\""
       << json_escape(config.worker_job_identity) << "\""
       << ",\"worker_job_total_processes\":"
       << config.worker_job_total_processes
       << ",\"worker_job_peak_active_processes\":"
       << config.worker_job_peak_active_processes
       << ",\"worker_job_total_terminated_processes\":"
       << config.worker_job_total_terminated_processes
       << ",\"worker_root_peak_working_set_bytes\":"
       << config.worker_root_peak_working_set_bytes
       << ",\"worker_root_peak_private_commit_bytes\":"
       << config.worker_root_peak_private_commit_bytes
       << ",\"worker_tree_peak_working_set_bytes\":"
       << config.worker_tree_peak_working_set_bytes
       << ",\"worker_tree_peak_private_commit_bytes\":"
       << config.worker_tree_peak_private_commit_bytes
       << ",\"worker_tree_sample_interval_milliseconds\":"
       << config.worker_tree_sample_interval_milliseconds
       << ",\"worker_read_bytes\":" << config.worker_read_bytes
       << ",\"worker_write_bytes\":" << config.worker_write_bytes
       << ",\"worker_output_bytes\":" << config.worker_output_bytes
       << ",\"worker_output_limit_bytes\":" << config.worker_output_limit_bytes
       << ",\"kv_accounting\":"
       << (config.kv_accounting ? "true" : "false")
       << ",\"kv_placement\":\"" << json_escape(config.kv_placement) << "\""
       << ",\"kv_compression\":\"" << json_escape(config.kv_compression)
       << "\""
       << ",\"quality_gate\":\"" << json_escape(config.quality_gate) << "\"";
  out_ << ",\"profitability_policy\":\""
       << json_escape(config.profitability_policy) << "\""
       << ",\"offload_policy\":\"" << json_escape(config.offload_policy)
       << "\""
       << ",\"claim_label\":\"" << json_escape(config.claim_label) << "\""
       << ",\"claim_validation\":\""
       << json_escape(config.claim_validation) << "\"";

  for (const auto& [key, value] : fields) {
    if (reserved_event_field(key)) {
      continue;
    }
    out_ << ",\"" << json_escape(key) << "\":\"" << json_escape(value)
         << "\"";
  }

  out_ << "}\n";
}

void JsonlTelemetry::emit_memory_sample(const RuntimeConfig& config,
                                        const MemorySample& sample,
                                        bool telemetry_agreement) {
  if (!ok()) {
    return;
  }

  out_ << "{\"schema_version\":\"0.6\""
       << ",\"timestamp_ns\":" << monotonic_time_ns()
       << ",\"run_id\":\"" << json_escape(config.run_id) << "\""
       << ",\"event\":\"memory_sample\""
       << ",\"mode\":\"" << json_escape(to_string(config.mode)) << "\""
       << ",\"hard_cap_bytes\":" << config.hard_cap_bytes
       << ",\"model_parameter_bucket\":\""
       << json_escape(config.model_parameter_bucket) << "\""
       << ",\"parameter_count\":" << config.parameter_count
       << ",\"vram_tier_gib\":" << config.vram_tier_gib
       << ",\"validation_cell_id\":\""
       << json_escape(config.validation_cell_id) << "\""
       << ",\"validation_cell_status\":\""
       << json_escape(config.validation_cell_status) << "\""
       << ",\"backend\":\"" << json_escape(to_string(config.backend)) << "\""
       << ",\"backend_required\":"
       << (config.backend_required ? "true" : "false")
       << ",\"dependency_pin_file\":\""
       << json_escape(config.dependency_pin_file.generic_string()) << "\""
       << ",\"llama_executable\":\""
       << json_escape(config.llama_executable_path.generic_string()) << "\""
       << ",\"context_tokens\":" << config.context_tokens
       << ",\"gpu_layers\":" << config.gpu_layers
       << ",\"mmap_enabled\":" << (config.mmap_enabled ? "true" : "false")
       << ",\"warmup_tokens\":" << config.warmup_tokens
       << ",\"worker_evidence_available\":"
       << (config.worker_evidence_available ? "true" : "false")
       << ",\"worker_executable_sha256\":\""
       << json_escape(config.worker_executable_sha256) << "\""
       << ",\"worker_approval_identity\":\""
       << json_escape(config.worker_approval_identity) << "\""
       << ",\"worker_exit_code\":" << config.worker_exit_code
       << ",\"worker_timed_out\":"
       << (config.worker_timed_out ? "true" : "false")
       << ",\"worker_root_process_id\":" << config.worker_root_process_id
       << ",\"worker_job_identity\":\""
       << json_escape(config.worker_job_identity) << "\""
       << ",\"worker_job_total_processes\":"
       << config.worker_job_total_processes
       << ",\"worker_job_peak_active_processes\":"
       << config.worker_job_peak_active_processes
       << ",\"worker_job_total_terminated_processes\":"
       << config.worker_job_total_terminated_processes
       << ",\"worker_root_peak_working_set_bytes\":"
       << config.worker_root_peak_working_set_bytes
       << ",\"worker_root_peak_private_commit_bytes\":"
       << config.worker_root_peak_private_commit_bytes
       << ",\"worker_tree_peak_working_set_bytes\":"
       << config.worker_tree_peak_working_set_bytes
       << ",\"worker_tree_peak_private_commit_bytes\":"
       << config.worker_tree_peak_private_commit_bytes
       << ",\"worker_tree_sample_interval_milliseconds\":"
       << config.worker_tree_sample_interval_milliseconds
       << ",\"worker_read_bytes\":" << config.worker_read_bytes
       << ",\"worker_write_bytes\":" << config.worker_write_bytes
       << ",\"worker_output_bytes\":" << config.worker_output_bytes
       << ",\"worker_output_limit_bytes\":" << config.worker_output_limit_bytes
       << ",\"kv_accounting\":"
       << (config.kv_accounting ? "true" : "false")
       << ",\"kv_placement\":\"" << json_escape(config.kv_placement) << "\""
       << ",\"kv_compression\":\"" << json_escape(config.kv_compression)
       << "\""
       << ",\"quality_gate\":\"" << json_escape(config.quality_gate) << "\""
       << ",\"profitability_policy\":\""
       << json_escape(config.profitability_policy) << "\""
       << ",\"offload_policy\":\"" << json_escape(config.offload_policy)
       << "\""
       << ",\"claim_label\":\"" << json_escape(config.claim_label) << "\""
       << ",\"claim_validation\":\""
       << json_escape(config.claim_validation)
       << "\""
       << ",\"allocator_bytes\":" << sample.allocator_bytes
       << ",\"allocator_peak_bytes\":" << sample.allocator_peak_bytes
       << ",\"process_gpu_bytes\":" << sample.process_gpu_bytes
       << ",\"process_gpu_peak_bytes\":" << sample.process_gpu_peak_bytes
       << ",\"warmup_peak_bytes\":" << sample.warmup_peak_bytes
       << ",\"retained_pool_bytes\":" << sample.retained_pool_bytes
       << ",\"kv_gpu_peak_bytes\":" << sample.kv_gpu_peak_bytes
       << ",\"kv_host_peak_bytes\":" << sample.kv_host_peak_bytes
       << ",\"kv_compressed_peak_bytes\":"
       << sample.kv_compressed_peak_bytes
       << ",\"kv_metadata_peak_bytes\":" << sample.kv_metadata_peak_bytes
       << ",\"kv_unknown_peak_bytes\":" << sample.kv_unknown_peak_bytes
       << ",\"device_used_bytes\":" << sample.device_used_bytes
       << ",\"device_delta_bytes\":" << sample.device_delta_bytes
       << ",\"unknown_post_warmup_bytes\":"
       << sample.unknown_post_warmup_bytes
       << ",\"telemetry_agreement\":"
       << (telemetry_agreement ? "true" : "false")
       << "}\n";
}

}  // namespace prisminfer

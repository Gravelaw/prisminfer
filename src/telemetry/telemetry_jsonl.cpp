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
         key == "dependency_pin_file" || key == "context_tokens" ||
         key == "gpu_layers" || key == "mmap_enabled" ||
         key == "warmup_tokens";
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

  out_ << "{\"schema_version\":\"0.2\""
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
       << ",\"context_tokens\":" << config.context_tokens
       << ",\"gpu_layers\":" << config.gpu_layers
       << ",\"mmap_enabled\":" << (config.mmap_enabled ? "true" : "false")
       << ",\"warmup_tokens\":" << config.warmup_tokens;

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

  out_ << "{\"schema_version\":\"0.2\""
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
       << ",\"context_tokens\":" << config.context_tokens
       << ",\"gpu_layers\":" << config.gpu_layers
       << ",\"mmap_enabled\":" << (config.mmap_enabled ? "true" : "false")
       << ",\"warmup_tokens\":" << config.warmup_tokens
       << ",\"allocator_bytes\":" << sample.allocator_bytes
       << ",\"allocator_peak_bytes\":" << sample.allocator_peak_bytes
       << ",\"process_gpu_bytes\":" << sample.process_gpu_bytes
       << ",\"process_gpu_peak_bytes\":" << sample.process_gpu_peak_bytes
       << ",\"warmup_peak_bytes\":" << sample.warmup_peak_bytes
       << ",\"retained_pool_bytes\":" << sample.retained_pool_bytes
       << ",\"device_used_bytes\":" << sample.device_used_bytes
       << ",\"device_delta_bytes\":" << sample.device_delta_bytes
       << ",\"unknown_post_warmup_bytes\":"
       << sample.unknown_post_warmup_bytes
       << ",\"telemetry_agreement\":"
       << (telemetry_agreement ? "true" : "false")
       << "}\n";
}

}  // namespace prisminfer

#include "prisminfer/telemetry.h"

#include <sstream>

namespace prisminfer {

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

  out_ << "{\"schema_version\":\"0.1\""
       << ",\"timestamp_ns\":" << monotonic_time_ns()
       << ",\"run_id\":\"" << json_escape(config.run_id) << "\""
       << ",\"event\":\"" << json_escape(event) << "\""
       << ",\"mode\":\"" << json_escape(to_string(config.mode)) << "\""
       << ",\"hard_cap_bytes\":" << config.hard_cap_bytes;

  for (const auto& [key, value] : fields) {
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

  out_ << "{\"schema_version\":\"0.1\""
       << ",\"timestamp_ns\":" << monotonic_time_ns()
       << ",\"run_id\":\"" << json_escape(config.run_id) << "\""
       << ",\"event\":\"memory_sample\""
       << ",\"mode\":\"" << json_escape(to_string(config.mode)) << "\""
       << ",\"hard_cap_bytes\":" << config.hard_cap_bytes
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

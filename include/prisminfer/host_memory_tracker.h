#pragma once

#include <cstdint>
#include <string>

namespace prisminfer {

struct HostTelemetrySample {
  bool available{false};
  std::string unavailable_reason;
  std::string system_commit_source;
  std::uint64_t captured_monotonic_milliseconds{0};
  std::uint32_t process_id{0};
  std::string process_image_path;
  std::uint64_t process_working_set_current_bytes{0};
  std::uint64_t process_working_set_peak_bytes{0};
  std::uint64_t process_private_commit_current_bytes{0};
  std::uint64_t process_private_commit_peak_bytes{0};
  std::uint64_t system_memory_total_bytes{0};
  std::uint64_t system_memory_available_bytes{0};
  std::uint64_t system_commit_total_bytes{0};
  std::uint64_t system_commit_limit_bytes{0};
  std::uint64_t system_commit_available_bytes{0};
  bool pagefile_configuration_available{false};
  std::string pagefile_configuration_unavailable_reason;
  std::uint32_t pagefile_count{0};
  std::uint64_t pagefile_total_bytes{0};
  std::uint64_t pagefile_current_usage_bytes{0};
  std::uint64_t pagefile_peak_usage_bytes{0};
  std::uint64_t process_io_read_bytes{0};
  std::uint64_t process_io_write_bytes{0};
};

HostTelemetrySample sample_host_telemetry();

}  // namespace prisminfer

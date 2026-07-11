#pragma once

#include <cstdint>
#include <string>

namespace prisminfer {

struct HostTelemetrySample {
  bool available{false};
  std::string unavailable_reason;
  std::string system_commit_source;
  std::uint64_t captured_monotonic_milliseconds{0};
  std::uint64_t process_working_set_bytes{0};
  std::uint64_t process_private_bytes{0};
  std::uint64_t system_memory_total_bytes{0};
  std::uint64_t system_memory_available_bytes{0};
  std::uint64_t system_commit_total_bytes{0};
  std::uint64_t system_commit_limit_bytes{0};
  std::uint64_t system_commit_available_bytes{0};
  std::uint64_t process_io_read_bytes{0};
  std::uint64_t process_io_write_bytes{0};
};

HostTelemetrySample sample_host_telemetry();

}  // namespace prisminfer

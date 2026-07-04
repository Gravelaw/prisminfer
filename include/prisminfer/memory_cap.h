#pragma once

#include <cstdint>
#include <string>

namespace prisminfer {

struct MemorySample {
  std::uint64_t allocator_bytes{0};
  std::uint64_t allocator_peak_bytes{0};
  std::uint64_t process_gpu_bytes{0};
  std::uint64_t process_gpu_peak_bytes{0};
  std::uint64_t warmup_peak_bytes{0};
  std::uint64_t retained_pool_bytes{0};
  std::uint64_t kv_gpu_peak_bytes{0};
  std::uint64_t kv_host_peak_bytes{0};
  std::uint64_t kv_compressed_peak_bytes{0};
  std::uint64_t kv_metadata_peak_bytes{0};
  std::uint64_t kv_unknown_peak_bytes{0};
  std::uint64_t device_used_bytes{0};
  std::uint64_t device_delta_bytes{0};
  std::uint64_t unknown_post_warmup_bytes{0};
  bool required_telemetry_present{true};
  bool telemetry_agreement{true};
  bool unified_memory_enabled{false};
};

struct CapTolerances {
  std::uint64_t allocator_process_tolerance_bytes{16ULL * 1024ULL * 1024ULL};
  std::uint64_t device_delta_tolerance_bytes{64ULL * 1024ULL * 1024ULL};
 };

struct CapCertificationResult {
  bool certified{false};
  std::string failure_reason;
};

CapCertificationResult certify_cap(const MemorySample& sample,
                                   std::uint64_t hard_cap_bytes);
CapCertificationResult certify_cap(const MemorySample& sample,
                                   std::uint64_t hard_cap_bytes,
                                   const CapTolerances& tolerances);

}  // namespace prisminfer

#pragma once

#include <cstdint>
#include <string>

namespace prisminfer {

struct CudaProbeResult {
  bool available{false};
  bool context_created{false};
  std::uint64_t context_bytes{0};
  std::uint64_t device_used_before_bytes{0};
  std::uint64_t device_used_after_bytes{0};
  std::uint64_t device_total_bytes{0};
  int driver_version{0};
  int runtime_version{0};
  std::string gpu_name;
  std::string failure_reason;
};

CudaProbeResult probe_cuda_context();

}  // namespace prisminfer

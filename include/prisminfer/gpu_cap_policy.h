#pragma once

#include <cstdint>
#include <string>

namespace prisminfer {

constexpr std::uint64_t kMaxGpuHardCapBytes = 17'179'869'184ULL;

struct GpuCapPolicyResult {
  bool accepted{true};
  std::string failure_reason;
};

GpuCapPolicyResult validate_gpu_hard_cap(std::uint64_t hard_cap_bytes);

}  // namespace prisminfer

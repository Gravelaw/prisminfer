#include "prisminfer/gpu_cap_policy.h"

namespace prisminfer {

GpuCapPolicyResult validate_gpu_hard_cap(std::uint64_t hard_cap_bytes) {
  if (hard_cap_bytes == 0) {
    return {false, "hard_cap_must_be_positive"};
  }
  if (hard_cap_bytes > kMaxGpuHardCapBytes) {
    return {false, "hard_cap_exceeds_max_gpu_cap"};
  }
  return {};
}

}  // namespace prisminfer

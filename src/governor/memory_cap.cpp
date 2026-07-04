#include "prisminfer/memory_cap.h"

namespace prisminfer {

CapCertificationResult certify_cap(const MemorySample& sample,
                                   std::uint64_t hard_cap_bytes) {
  return certify_cap(sample, hard_cap_bytes, CapTolerances{});
}

namespace {

std::uint64_t abs_diff(std::uint64_t lhs, std::uint64_t rhs) {
  return lhs > rhs ? lhs - rhs : rhs - lhs;
}

}  // namespace

CapCertificationResult certify_cap(const MemorySample& sample,
                                   std::uint64_t hard_cap_bytes,
                                   const CapTolerances& tolerances) {
  if (!sample.required_telemetry_present) {
    return {false, "required_telemetry_missing"};
  }
  if (!sample.telemetry_agreement) {
    return {false, "telemetry_disagreement"};
  }
  if (sample.unified_memory_enabled) {
    return {false, "unified_memory_enabled"};
  }
  if (sample.allocator_peak_bytes > hard_cap_bytes) {
    return {false, "allocator_peak_exceeded_cap"};
  }
  if (sample.process_gpu_peak_bytes > hard_cap_bytes) {
    return {false, "process_gpu_peak_exceeded_cap"};
  }
  if (sample.warmup_peak_bytes > hard_cap_bytes) {
    return {false, "warmup_peak_exceeded_cap"};
  }
  if (sample.kv_gpu_peak_bytes > hard_cap_bytes) {
    return {false, "kv_gpu_peak_exceeded_cap"};
  }
  if (sample.unknown_post_warmup_bytes > 0) {
    return {false, "unknown_post_warmup_allocation"};
  }
  if (abs_diff(sample.allocator_peak_bytes, sample.process_gpu_peak_bytes) >
      tolerances.allocator_process_tolerance_bytes) {
    return {false, "allocator_process_peak_disagreement"};
  }
  if (sample.device_delta_bytes >
          sample.process_gpu_peak_bytes +
              tolerances.device_delta_tolerance_bytes &&
      sample.process_gpu_peak_bytes > 0) {
    return {false, "device_delta_process_disagreement"};
  }
  return {true, ""};
}

}  // namespace prisminfer

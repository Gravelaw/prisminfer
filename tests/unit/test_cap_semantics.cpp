#include <iostream>
#include <string>

#include "prisminfer/memory_cap.h"

namespace {

int expect(bool condition, const char* message) {
  if (condition) {
    return 0;
  }
  std::cerr << "FAIL: " << message << "\n";
  return 1;
}

int expect_eq(const std::string& actual,
              const std::string& expected,
              const char* message) {
  if (actual == expected) {
    return 0;
  }
  std::cerr << "FAIL: " << message << ": expected " << expected
            << ", got " << actual << "\n";
  return 1;
}

}  // namespace

int main() {
  {
    prisminfer::MemorySample sample;
    const auto result = prisminfer::certify_cap(sample, 1024);
    if (expect(result.certified, "empty sample certifies under cap")) return 1;
  }
  {
    prisminfer::MemorySample sample;
    sample.allocator_peak_bytes = 2048;
    const auto result = prisminfer::certify_cap(sample, 1024);
    if (expect(!result.certified, "allocator breach fails")) return 1;
    if (expect_eq(result.failure_reason, "allocator_peak_exceeded_cap",
                  "allocator breach reason")) return 1;
  }
  {
    prisminfer::MemorySample sample;
    sample.process_gpu_peak_bytes = 2048;
    const auto result = prisminfer::certify_cap(sample, 1024);
    if (expect(!result.certified, "process GPU breach fails")) return 1;
    if (expect_eq(result.failure_reason, "process_gpu_peak_exceeded_cap",
                  "process GPU breach reason")) return 1;
  }
  {
    prisminfer::MemorySample sample;
    sample.warmup_peak_bytes = 2048;
    const auto result = prisminfer::certify_cap(sample, 1024);
    if (expect(!result.certified, "warmup breach fails")) return 1;
    if (expect_eq(result.failure_reason, "warmup_peak_exceeded_cap",
                  "warmup breach reason")) return 1;
  }
  {
    prisminfer::MemorySample sample;
    sample.required_telemetry_present = false;
    const auto result = prisminfer::certify_cap(sample, 1024);
    if (expect(!result.certified, "missing telemetry fails")) return 1;
    if (expect_eq(result.failure_reason, "required_telemetry_missing",
                  "missing telemetry reason")) return 1;
  }
  {
    prisminfer::MemorySample sample;
    sample.unknown_post_warmup_bytes = 1;
    const auto result = prisminfer::certify_cap(sample, 1024);
    if (expect(!result.certified, "unknown post-warmup allocation fails")) {
      return 1;
    }
    if (expect_eq(result.failure_reason, "unknown_post_warmup_allocation",
                  "unknown post-warmup reason")) return 1;
  }
  {
    prisminfer::MemorySample sample;
    sample.allocator_peak_bytes = 128;
    sample.process_gpu_peak_bytes = 512;
    prisminfer::CapTolerances tolerances;
    tolerances.allocator_process_tolerance_bytes = 16;
    const auto result = prisminfer::certify_cap(sample, 1024, tolerances);
    if (expect(!result.certified, "allocator/process disagreement fails")) {
      return 1;
    }
    if (expect_eq(result.failure_reason,
                  "allocator_process_peak_disagreement",
                  "allocator/process disagreement reason")) return 1;
  }
  {
    prisminfer::MemorySample sample;
    sample.allocator_peak_bytes = 512;
    sample.process_gpu_peak_bytes = 512;
    sample.device_delta_bytes = 900;
    prisminfer::CapTolerances tolerances;
    tolerances.device_delta_tolerance_bytes = 16;
    const auto result = prisminfer::certify_cap(sample, 1024, tolerances);
    if (expect(!result.certified, "device/process disagreement fails")) {
      return 1;
    }
    if (expect_eq(result.failure_reason,
                  "device_delta_process_disagreement",
                  "device/process disagreement reason")) return 1;
  }
  return 0;
}

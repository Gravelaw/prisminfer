#pragma once

#include <cstdint>
#include <string>

#include "prisminfer/memory_cap.h"

namespace prisminfer {

struct AllocationResult {
  bool accepted{false};
  std::string failure_reason;
};

class CappedAllocatorTracker {
 public:
  explicit CappedAllocatorTracker(std::uint64_t hard_cap_bytes);

  AllocationResult reserve(std::uint64_t bytes);
  AllocationResult release(std::uint64_t bytes);
  void observe_rejected_peak(std::uint64_t attempted_peak_bytes);

  std::uint64_t hard_cap_bytes() const;
  std::uint64_t current_bytes() const;
  std::uint64_t peak_bytes() const;

  MemorySample sample() const;

 private:
  std::uint64_t hard_cap_bytes_{0};
  std::uint64_t current_bytes_{0};
  std::uint64_t peak_bytes_{0};
  std::uint64_t rejected_peak_bytes_{0};
};

}  // namespace prisminfer


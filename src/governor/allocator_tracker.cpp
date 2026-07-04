#include "prisminfer/allocator_tracker.h"

#include <algorithm>
#include <limits>

namespace prisminfer {

CappedAllocatorTracker::CappedAllocatorTracker(std::uint64_t hard_cap_bytes)
    : hard_cap_bytes_(hard_cap_bytes) {}

AllocationResult CappedAllocatorTracker::reserve(std::uint64_t bytes) {
  if (bytes > std::numeric_limits<std::uint64_t>::max() - current_bytes_) {
    rejected_peak_bytes_ = std::numeric_limits<std::uint64_t>::max();
    return {false, "allocator_size_overflow"};
  }

  const std::uint64_t attempted = current_bytes_ + bytes;
  if (attempted > hard_cap_bytes_) {
    observe_rejected_peak(attempted);
    return {false, "allocator_cap_would_be_exceeded"};
  }

  current_bytes_ = attempted;
  peak_bytes_ = std::max(peak_bytes_, current_bytes_);
  return {true, ""};
}

AllocationResult CappedAllocatorTracker::release(std::uint64_t bytes) {
  if (bytes > current_bytes_) {
    return {false, "allocator_release_exceeds_current"};
  }
  current_bytes_ -= bytes;
  return {true, ""};
}

void CappedAllocatorTracker::observe_rejected_peak(
    std::uint64_t attempted_peak_bytes) {
  rejected_peak_bytes_ = std::max(rejected_peak_bytes_, attempted_peak_bytes);
}

std::uint64_t CappedAllocatorTracker::hard_cap_bytes() const {
  return hard_cap_bytes_;
}

std::uint64_t CappedAllocatorTracker::current_bytes() const {
  return current_bytes_;
}

std::uint64_t CappedAllocatorTracker::peak_bytes() const {
  return std::max(peak_bytes_, rejected_peak_bytes_);
}

MemorySample CappedAllocatorTracker::sample() const {
  MemorySample sample;
  sample.allocator_bytes = current_bytes_;
  sample.allocator_peak_bytes = peak_bytes();
  return sample;
}

}  // namespace prisminfer


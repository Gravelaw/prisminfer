#include <iostream>
#include <string>

#include "prisminfer/allocator_tracker.h"

namespace {

int expect(bool condition, const char* message) {
  if (condition) {
    return 0;
  }
  std::cerr << "FAIL: " << message << "\n";
  return 1;
}

int expect_eq(std::uint64_t actual,
              std::uint64_t expected,
              const char* message) {
  if (actual == expected) {
    return 0;
  }
  std::cerr << "FAIL: " << message << ": expected " << expected << ", got "
            << actual << "\n";
  return 1;
}

int expect_eq(const std::string& actual,
              const std::string& expected,
              const char* message) {
  if (actual == expected) {
    return 0;
  }
  std::cerr << "FAIL: " << message << ": expected " << expected << ", got "
            << actual << "\n";
  return 1;
}

}  // namespace

int main() {
  prisminfer::CappedAllocatorTracker tracker(1024);

  auto result = tracker.reserve(256);
  if (expect(result.accepted, "reserve under cap succeeds")) return 1;
  if (expect_eq(tracker.current_bytes(), 256, "current bytes after reserve")) {
    return 1;
  }
  if (expect_eq(tracker.peak_bytes(), 256, "peak bytes after reserve")) {
    return 1;
  }

  result = tracker.release(128);
  if (expect(result.accepted, "release under current succeeds")) return 1;
  if (expect_eq(tracker.current_bytes(), 128, "current bytes after release")) {
    return 1;
  }
  if (expect_eq(tracker.peak_bytes(), 256, "peak remains high-water mark")) {
    return 1;
  }

  result = tracker.reserve(1024);
  if (expect(!result.accepted, "reserve over cap rejected")) return 1;
  if (expect_eq(result.failure_reason, "allocator_cap_would_be_exceeded",
                "reserve over cap reason")) return 1;
  if (expect_eq(tracker.current_bytes(), 128,
                "rejected reserve does not change current")) return 1;
  if (expect_eq(tracker.peak_bytes(), 1152,
                "rejected reserve records attempted peak")) return 1;

  result = tracker.release(2048);
  if (expect(!result.accepted, "over-release rejected")) return 1;
  if (expect_eq(result.failure_reason, "allocator_release_exceeds_current",
                "over-release reason")) return 1;

  const auto sample = tracker.sample();
  if (expect_eq(sample.allocator_bytes, 128, "sample current bytes")) return 1;
  if (expect_eq(sample.allocator_peak_bytes, 1152, "sample peak bytes")) {
    return 1;
  }
  return 0;
}


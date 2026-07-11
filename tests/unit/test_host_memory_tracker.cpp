#include <iostream>

#include "prisminfer/host_memory_tracker.h"

namespace {

int expect(bool condition, const char* message) {
  if (condition) {
    return 0;
  }
  std::cerr << "FAIL: " << message << "\n";
  return 1;
}

}  // namespace

int main() {
  const auto sample = prisminfer::sample_host_telemetry();
  if (sample.available) {
    if (expect(sample.process_working_set_bytes > 0,
               "working set is recorded when host telemetry is available")) {
      return 1;
    }
    if (expect(sample.system_commit_source == "get_performance_info",
               "system commit uses the authoritative Windows source") ||
        expect(sample.captured_monotonic_milliseconds > 0,
               "host sample has monotonic capture provenance") ||
        expect(sample.system_memory_total_bytes > 0,
               "physical total is recorded") ||
        expect(sample.system_memory_available_bytes <=
                   sample.system_memory_total_bytes,
               "available physical memory does not exceed total") ||
        expect(sample.system_commit_total_bytes <=
                   sample.system_commit_limit_bytes,
               "system commit charge does not exceed limit") ||
        expect(sample.system_commit_available_bytes ==
                   sample.system_commit_limit_bytes -
                       sample.system_commit_total_bytes,
               "system commit headroom is reconciled")) {
      return 1;
    }
  } else {
    if (expect(!sample.unavailable_reason.empty(),
               "unavailable host telemetry has a reason")) {
      return 1;
    }
  }
  return 0;
}

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
    if (expect(sample.process_id != 0U && !sample.process_image_path.empty(),
               "process identity is recorded") ||
        expect(sample.process_working_set_current_bytes > 0,
               "working set is recorded when host telemetry is available")) {
      return 1;
    }
    if (expect(sample.process_working_set_peak_bytes >=
                   sample.process_working_set_current_bytes,
               "working-set peak is not lower than current") ||
        expect(sample.process_private_commit_peak_bytes >=
                   sample.process_private_commit_current_bytes,
               "private-commit peak is not lower than current")) {
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
    if (sample.pagefile_configuration_available) {
      if (expect(sample.pagefile_current_usage_bytes <=
                         sample.pagefile_peak_usage_bytes &&
                     sample.pagefile_peak_usage_bytes <=
                         sample.pagefile_total_bytes,
                 "pagefile configuration counters reconcile")) {
        return 1;
      }
    } else if (expect(
                   !sample.pagefile_configuration_unavailable_reason.empty(),
                   "unavailable pagefile configuration has a typed reason")) {
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

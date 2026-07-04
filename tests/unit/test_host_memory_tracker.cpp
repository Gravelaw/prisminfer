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
  } else {
    if (expect(!sample.unavailable_reason.empty(),
               "unavailable host telemetry has a reason")) {
      return 1;
    }
  }
  return 0;
}

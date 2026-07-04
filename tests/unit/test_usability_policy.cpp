#include <iostream>

#include "prisminfer/usability_policy.h"

namespace {
int expect(bool condition, const char* message) {
  if (condition) return 0;
  std::cerr << "FAIL: " << message << "\n";
  return 1;
}
}  // namespace

int main() {
  prisminfer::UsabilityInput input;
  input.time_to_first_token_ms = 1000.0;
  input.decode_tokens_per_second = 4.0;
  input.token_latency_p95_ms = 300.0;
  input.max_time_to_first_token_ms = 2000.0;
  input.min_decode_tokens_per_second = 2.0;
  input.max_token_latency_p95_ms = 500.0;
  input.repeatability_runs = 3;
  input.repeatability_passed = true;
  if (expect(prisminfer::evaluate_usability(input).accepted,
             "usable run accepted")) return 1;
  input.decode_tokens_per_second = 1.0;
  const auto slow = prisminfer::evaluate_usability(input);
  if (expect(!slow.accepted, "slow run rejected")) return 1;
  if (expect(slow.reason == "decode_throughput_below_threshold",
             "slow reason")) return 1;
  return 0;
}


#include "prisminfer/usability_policy.h"

namespace prisminfer {

UsabilityResult evaluate_usability(const UsabilityInput& input) {
  if (input.max_time_to_first_token_ms <= 0.0 ||
      input.min_decode_tokens_per_second <= 0.0 ||
      input.max_token_latency_p95_ms <= 0.0) {
    return {false, "rejected", "usability_thresholds_required"};
  }
  if (input.time_to_first_token_ms <= 0.0 ||
      input.decode_tokens_per_second <= 0.0 ||
      input.token_latency_p95_ms <= 0.0) {
    return {false, "rejected", "usability_measurements_required"};
  }
  if (input.time_to_first_token_ms > input.max_time_to_first_token_ms) {
    return {false, "rejected", "time_to_first_token_exceeded"};
  }
  if (input.decode_tokens_per_second < input.min_decode_tokens_per_second) {
    return {false, "rejected", "decode_throughput_below_threshold"};
  }
  if (input.token_latency_p95_ms > input.max_token_latency_p95_ms) {
    return {false, "rejected", "p95_latency_exceeded"};
  }
  if (input.repeatability_runs < input.required_repeatability_runs ||
      !input.repeatability_passed) {
    return {false, "rejected", "repeatability_required"};
  }
  return {true, "accepted", ""};
}

}  // namespace prisminfer


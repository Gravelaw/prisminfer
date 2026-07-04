#pragma once

#include <cstdint>
#include <string>

namespace prisminfer {

struct UsabilityInput {
  double time_to_first_token_ms{0.0};
  double decode_tokens_per_second{0.0};
  double token_latency_p95_ms{0.0};
  double max_time_to_first_token_ms{0.0};
  double min_decode_tokens_per_second{0.0};
  double max_token_latency_p95_ms{0.0};
  std::uint32_t repeatability_runs{0};
  std::uint32_t required_repeatability_runs{3};
  bool repeatability_passed{false};
};

struct UsabilityResult {
  bool accepted{false};
  std::string status{"rejected"};
  std::string reason;
};

UsabilityResult evaluate_usability(const UsabilityInput& input);

}  // namespace prisminfer


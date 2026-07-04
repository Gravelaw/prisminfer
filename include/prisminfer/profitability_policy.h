#pragma once

#include <string>

#include "prisminfer/transfer_metrics.h"

namespace prisminfer {

struct ProfitabilityBaseline {
  double cpu_time_to_first_token_ms{0.0};
  double cpu_decode_tokens_per_second{0.0};
  std::uint64_t cpu_peak_bytes{0};
  std::string validation_cell_id;
};

struct ProfitabilityInput {
  std::string policy{"off"};
  std::string offload_policy{"none"};
  double min_speedup_ratio{1.1};
  ProfitabilityBaseline baseline;
  TransferSample candidate;
  std::string candidate_validation_cell_id;
  bool cap_certified{true};
  bool quality_passed{true};
  bool transfer_metrics_present{false};
};

struct ProfitabilityDecision {
  bool accepted{true};
  std::string status{"skipped"};
  std::string reason;
  double speedup_ratio{0.0};
  double required_speedup_ratio{1.1};
};

bool valid_profitability_policy(const std::string& policy);
ProfitabilityDecision evaluate_profitability(const ProfitabilityInput& input);

}  // namespace prisminfer


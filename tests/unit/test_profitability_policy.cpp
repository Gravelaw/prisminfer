#include <iostream>

#include "prisminfer/profitability_policy.h"

namespace {

int expect(bool condition, const char* message) {
  if (condition) return 0;
  std::cerr << "FAIL: " << message << "\n";
  return 1;
}

prisminfer::ProfitabilityInput base_input() {
  prisminfer::ProfitabilityInput input;
  input.policy = "fail-closed";
  input.offload_policy = "gpu";
  input.min_speedup_ratio = 1.1;
  input.baseline.cpu_time_to_first_token_ms = 100.0;
  input.baseline.cpu_decode_tokens_per_second = 10.0;
  input.baseline.validation_cell_id = "cell";
  input.candidate.time_to_first_token_ms = 80.0;
  input.candidate.decode_tokens_per_second = 12.0;
  input.candidate_validation_cell_id = "cell";
  input.transfer_metrics_present = true;
  return input;
}

}  // namespace

int main() {
  const auto accepted = prisminfer::evaluate_profitability(base_input());
  if (expect(accepted.accepted, "profitable candidate accepted")) return 1;
  if (expect(accepted.status == "accepted", "accepted status")) return 1;

  auto slow = base_input();
  slow.candidate.decode_tokens_per_second = 10.1;
  slow.candidate.time_to_first_token_ms = 99.0;
  const auto rejected = prisminfer::evaluate_profitability(slow);
  if (expect(!rejected.accepted, "slow candidate rejected")) return 1;
  if (expect(rejected.reason == "offload_not_profitable",
             "slow candidate reason")) return 1;

  auto mismatch = base_input();
  mismatch.candidate_validation_cell_id = "other-cell";
  const auto mismatched = prisminfer::evaluate_profitability(mismatch);
  if (expect(!mismatched.accepted, "cell mismatch rejected")) return 1;
  if (expect(mismatched.reason == "validation_cell_mismatch",
             "cell mismatch reason")) return 1;
  return 0;
}


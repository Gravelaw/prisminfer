#include "prisminfer/profitability_policy.h"

namespace prisminfer {
namespace {

double throughput_speedup(double baseline_tps, double candidate_tps) {
  if (baseline_tps <= 0.0 || candidate_tps <= 0.0) {
    return 0.0;
  }
  return candidate_tps / baseline_tps;
}

double ttft_speedup(double baseline_ms, double candidate_ms) {
  if (baseline_ms <= 0.0 || candidate_ms <= 0.0) {
    return 0.0;
  }
  return baseline_ms / candidate_ms;
}

}  // namespace

bool valid_profitability_policy(const std::string& policy) {
  return policy == "off" || policy == "warn" || policy == "fail-closed";
}

ProfitabilityDecision evaluate_profitability(const ProfitabilityInput& input) {
  if (!valid_profitability_policy(input.policy)) {
    return {false, "rejected", "profitability_policy_invalid", 0.0,
            input.min_speedup_ratio};
  }
  if (input.policy == "off") {
    return {true, "skipped", "", 0.0, input.min_speedup_ratio};
  }
  if (!input.cap_certified) {
    return {false, "rejected", "cap_certification_required", 0.0,
            input.min_speedup_ratio};
  }
  if (!input.quality_passed) {
    return {false, "rejected", "quality_gate_required_for_profitability", 0.0,
            input.min_speedup_ratio};
  }
  if (input.baseline.validation_cell_id.empty() ||
      input.candidate_validation_cell_id.empty()) {
    return {false, "rejected", "validation_cell_required", 0.0,
            input.min_speedup_ratio};
  }
  if (input.baseline.validation_cell_id != input.candidate_validation_cell_id) {
    return {false, "rejected", "validation_cell_mismatch", 0.0,
            input.min_speedup_ratio};
  }
  if (input.offload_policy != "none" && !input.transfer_metrics_present) {
    return {false, "rejected", "transfer_metrics_required", 0.0,
            input.min_speedup_ratio};
  }

  double speedup = throughput_speedup(
      input.baseline.cpu_decode_tokens_per_second,
      input.candidate.decode_tokens_per_second);
  const double ttft = ttft_speedup(input.baseline.cpu_time_to_first_token_ms,
                                  input.candidate.time_to_first_token_ms);
  if (ttft > speedup) {
    speedup = ttft;
  }
  if (speedup <= 0.0) {
    return {false, "rejected", "baseline_and_candidate_metrics_required", 0.0,
            input.min_speedup_ratio};
  }
  if (speedup < input.min_speedup_ratio) {
    return {false, input.policy == "warn" ? "warn" : "rejected",
            "offload_not_profitable", speedup, input.min_speedup_ratio};
  }
  return {true, "accepted", "", speedup, input.min_speedup_ratio};
}

}  // namespace prisminfer


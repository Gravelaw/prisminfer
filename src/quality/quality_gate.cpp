#include "prisminfer/quality_gate.h"

#include <cmath>

namespace prisminfer {

bool valid_quality_gate(const std::string& gate) {
  return gate == "none" || gate == "smoke" || gate == "retrieval" ||
         gate == "long-context";
}

QualityGateResult evaluate_quality_gate(const QualityGateInput& input) {
  if (!valid_quality_gate(input.gate)) {
    return {false, "failed", "quality_gate_invalid", 0.0};
  }
  if (input.gate == "none") {
    return {true, "skipped", "", 0.0};
  }

  const double delta = std::fabs(input.observed_score - input.baseline_score);
  if (!input.deterministic_match) {
    return {false, "failed", "deterministic_prompt_mismatch", delta};
  }
  if ((input.gate == "retrieval" || input.gate == "long-context") &&
      !input.retrieval_passed) {
    return {false, "failed", "retrieval_gate_failed", delta};
  }
  if (delta > input.max_delta) {
    return {false, "failed", "quality_delta_exceeded", delta};
  }
  return {true, "passed", "", delta};
}

}  // namespace prisminfer

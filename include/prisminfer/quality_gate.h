#pragma once

#include <string>

namespace prisminfer {

struct QualityGateInput {
  std::string gate{"none"};
  double baseline_score{1.0};
  double observed_score{1.0};
  double max_delta{0.0};
  bool deterministic_match{true};
  bool retrieval_passed{true};
};

struct QualityGateResult {
  bool passed{true};
  std::string status{"skipped"};
  std::string failure_reason;
  double observed_delta{0.0};
};

bool valid_quality_gate(const std::string& gate);
QualityGateResult evaluate_quality_gate(const QualityGateInput& input);

}  // namespace prisminfer

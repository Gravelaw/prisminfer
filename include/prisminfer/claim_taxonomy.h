#pragma once

#include <string>

namespace prisminfer {

struct ClaimEvidence {
  std::string claim_label{"metadata-only"};
  std::string validation_cell_id;
  std::string model_hash;
  std::string quantization_format;
  std::string quant_artifact_sha256;
  bool simulated_evidence{false};
  bool measured_evidence{false};
  bool cap_passed{false};
  bool quality_passed{false};
  bool profitability_passed{false};
  bool usability_passed{false};
  bool repeatability_passed{false};
  bool runbook_present{false};
};

struct ClaimDecision {
  bool accepted{false};
  std::string status{"rejected"};
  std::string reason;
};

bool valid_claim_label(const std::string& label);
ClaimDecision validate_claim_evidence(const ClaimEvidence& evidence);

}  // namespace prisminfer


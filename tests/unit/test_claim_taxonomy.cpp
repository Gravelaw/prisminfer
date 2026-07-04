#include <iostream>

#include "prisminfer/claim_taxonomy.h"

namespace {
int expect(bool condition, const char* message) {
  if (condition) return 0;
  std::cerr << "FAIL: " << message << "\n";
  return 1;
}
}  // namespace

int main() {
  prisminfer::ClaimEvidence simulated;
  simulated.claim_label = "simulated";
  simulated.validation_cell_id = "cell";
  simulated.simulated_evidence = true;
  if (expect(prisminfer::validate_claim_evidence(simulated).accepted,
             "simulated claim accepted with simulated evidence")) return 1;

  prisminfer::ClaimEvidence overclaim = simulated;
  overclaim.claim_label = "deployable-profile";
  const auto rejected = prisminfer::validate_claim_evidence(overclaim);
  if (expect(!rejected.accepted, "simulated deployable rejected")) return 1;
  if (expect(rejected.reason == "model_hash_required",
             "deployable needs model hash first")) return 1;

  prisminfer::ClaimEvidence validated;
  validated.claim_label = "validated-benchmark";
  validated.validation_cell_id = "cell";
  validated.model_hash = "abc";
  validated.quantization_format = "Q4_K_M";
  validated.quant_artifact_sha256 = "def";
  validated.measured_evidence = true;
  validated.cap_passed = true;
  validated.quality_passed = true;
  validated.profitability_passed = true;
  validated.usability_passed = true;
  validated.repeatability_passed = true;
  if (expect(prisminfer::validate_claim_evidence(validated).accepted,
             "validated benchmark accepted")) return 1;
  return 0;
}


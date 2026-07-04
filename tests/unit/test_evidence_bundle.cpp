#include <iostream>

#include "prisminfer/evidence_bundle.h"

namespace {
int expect(bool condition, const char* message) {
  if (condition) return 0;
  std::cerr << "FAIL: " << message << "\n";
  return 1;
}
}  // namespace

int main() {
  prisminfer::EvidenceBundle bundle;
  bundle.claim_label = "deployable-profile";
  bundle.validation_cell_id = "cell";
  bundle.hard_vram_cap_bytes = 17179869184ULL;
  bundle.manifest_present = true;
  bundle.telemetry_present = true;
  bundle.simulated_evidence = true;
  const auto rejected = prisminfer::validate_evidence_bundle(bundle);
  if (expect(!rejected.accepted, "simulated deployable bundle rejected")) {
    return 1;
  }

  bundle.claim_label = "validated-benchmark";
  bundle.model_hash_present = true;
  bundle.quant_hash_present = true;
  bundle.simulated_evidence = false;
  bundle.measured_evidence = true;
  bundle.quality_present = true;
  bundle.profitability_present = true;
  bundle.repeatability_present = true;
  if (expect(prisminfer::validate_evidence_bundle(bundle).accepted,
             "validated bundle accepted")) return 1;
  return 0;
}


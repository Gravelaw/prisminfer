#include "prisminfer/evidence_bundle.h"

namespace prisminfer {

ClaimDecision validate_evidence_bundle(const EvidenceBundle& bundle) {
  if (bundle.validation_cell_id.empty()) {
    return {false, "rejected", "validation_cell_required"};
  }
  if (bundle.hard_vram_cap_bytes == 0 ||
      bundle.hard_vram_cap_bytes > 17'179'869'184ULL) {
    return {false, "rejected", "hard_vram_cap_out_of_scope"};
  }
  if (!bundle.manifest_present || !bundle.telemetry_present) {
    return {false, "rejected", "manifest_and_telemetry_required"};
  }
  ClaimEvidence evidence;
  evidence.claim_label = bundle.claim_label;
  evidence.validation_cell_id = bundle.validation_cell_id;
  evidence.model_hash = bundle.model_hash_present ? "present" : "";
  evidence.quantization_format = bundle.quant_hash_present ? "present" : "";
  evidence.quant_artifact_sha256 = bundle.quant_hash_present ? "present" : "";
  evidence.simulated_evidence = bundle.simulated_evidence;
  evidence.measured_evidence = bundle.measured_evidence;
  evidence.cap_passed = true;
  evidence.quality_passed = bundle.quality_present;
  evidence.profitability_passed = bundle.profitability_present;
  evidence.usability_passed = bundle.profitability_present;
  evidence.repeatability_passed = bundle.repeatability_present;
  evidence.runbook_present = bundle.runbook_present;
  return validate_claim_evidence(evidence);
}

}  // namespace prisminfer


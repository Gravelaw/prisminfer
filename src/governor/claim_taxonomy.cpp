#include "prisminfer/claim_taxonomy.h"

namespace prisminfer {

bool valid_claim_label(const std::string& label) {
  return label == "metadata-only" || label == "simulated" ||
         label == "measured-offload" || label == "validated-benchmark" ||
         label == "deployable-profile" || label == "rejected";
}

ClaimDecision validate_claim_evidence(const ClaimEvidence& evidence) {
  if (!valid_claim_label(evidence.claim_label)) {
    return {false, "rejected", "claim_label_invalid"};
  }
  if (evidence.claim_label == "rejected") {
    return {true, "rejected", ""};
  }
  if (evidence.validation_cell_id.empty()) {
    return {false, "rejected", "validation_cell_required"};
  }
  if (evidence.claim_label == "metadata-only") {
    return {true, "accepted", ""};
  }
  if (evidence.claim_label == "simulated") {
    return evidence.simulated_evidence
               ? ClaimDecision{true, "accepted", ""}
               : ClaimDecision{false, "rejected", "simulated_evidence_required"};
  }
  if (evidence.model_hash.empty()) {
    return {false, "rejected", "model_hash_required"};
  }
  if (evidence.quantization_format.empty() ||
      evidence.quant_artifact_sha256.empty()) {
    return {false, "rejected", "quantization_evidence_required"};
  }
  if (evidence.claim_label == "measured-offload") {
    return evidence.measured_evidence
               ? ClaimDecision{true, "accepted", ""}
               : ClaimDecision{false, "rejected", "measured_evidence_required"};
  }
  if (evidence.simulated_evidence && !evidence.measured_evidence) {
    return {false, "rejected", "simulated_evidence_cannot_validate_claim"};
  }
  if (!evidence.cap_passed) {
    return {false, "rejected", "cap_evidence_required"};
  }
  if (!evidence.quality_passed) {
    return {false, "rejected", "quality_evidence_required"};
  }
  if (!evidence.profitability_passed) {
    return {false, "rejected", "profitability_evidence_required"};
  }
  if (!evidence.usability_passed) {
    return {false, "rejected", "usability_evidence_required"};
  }
  if (!evidence.repeatability_passed) {
    return {false, "rejected", "repeatability_evidence_required"};
  }
  if (evidence.claim_label == "deployable-profile" &&
      !evidence.runbook_present) {
    return {false, "rejected", "deployment_runbook_required"};
  }
  return {true, "accepted", ""};
}

}  // namespace prisminfer


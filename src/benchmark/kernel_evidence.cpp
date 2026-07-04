#include "prisminfer/kernel_evidence.h"

namespace prisminfer {

KernelEvidenceDecision validate_kernel_evidence(
    const KernelEvidence& evidence) {
  if (!evidence.schema_valid || evidence.unknown_fields_present) {
    return {false, "rejected", "kernel_schema_required"};
  }
  if (evidence.full_dequant_materialized) {
    return {false, "rejected", "full_dequant_materialized"};
  }
  if (!evidence.correctness_passed) {
    return {false, "rejected", "cpu_reference_correctness_required"};
  }
  if (!evidence.quality_fixture_present || !evidence.quality_passed) {
    return {false, "rejected", "quality_fixture_required"};
  }
  if (!evidence.same_cell_baseline) {
    return {false, "rejected", "same_cell_baseline_required"};
  }
  if (evidence.cuda_involved && !evidence.llama_ggml_cuda_baseline_present) {
    return {false, "rejected", "llama_ggml_cuda_baseline_required"};
  }
  if (evidence.vendor_baseline_required && !evidence.vendor_baseline_present) {
    return {false, "rejected", "vendor_baseline_required"};
  }
  if (evidence.tensor_core_claimed &&
      !evidence.tensor_core_profiler_artifact_present) {
    return {false, "rejected", "tensor_core_profiler_required"};
  }
  if (!evidence.cap_accounted && !evidence.measured_non_certified) {
    return {false, "rejected", "allocation_reconciliation_required"};
  }
  if (evidence.hard_cap_bytes > 0 &&
      evidence.workspace_peak_bytes > evidence.hard_cap_bytes) {
    return {false, "rejected", "workspace_exceeds_hard_cap"};
  }
  if (evidence.speedup_ratio < evidence.required_speedup_ratio) {
    return {false, "rejected", "kernel_not_faster_than_baseline"};
  }
  if (evidence.measured_non_certified) {
    return {true, "measured-non-certified", "accepted_non_certified"};
  }
  return {true, "measured", "accepted"};
}

}  // namespace prisminfer

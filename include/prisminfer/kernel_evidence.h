#pragma once

#include <cstdint>
#include <string>

namespace prisminfer {

struct KernelEvidence {
  bool schema_valid{false};
  bool unknown_fields_present{false};
  bool correctness_passed{false};
  bool quality_fixture_present{false};
  bool quality_passed{false};
  bool same_cell_baseline{false};
  bool cap_accounted{false};
  bool measured_non_certified{false};
  bool full_dequant_materialized{false};
  bool tensor_core_claimed{false};
  bool tensor_core_profiler_artifact_present{false};
  bool cuda_involved{false};
  bool llama_ggml_cuda_baseline_present{false};
  bool vendor_baseline_required{false};
  bool vendor_baseline_present{false};
  std::uint64_t workspace_peak_bytes{0};
  std::uint64_t hard_cap_bytes{0};
  double speedup_ratio{0.0};
  double required_speedup_ratio{1.1};
};

struct KernelEvidenceDecision {
  bool accepted{false};
  std::string status{"rejected"};
  std::string reason;
};

KernelEvidenceDecision validate_kernel_evidence(
    const KernelEvidence& evidence);

}  // namespace prisminfer

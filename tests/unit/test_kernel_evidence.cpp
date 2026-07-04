#include <iostream>

#include "prisminfer/kernel_evidence.h"

namespace {
int expect(bool condition, const char* message) {
  if (condition) return 0;
  std::cerr << "FAIL: " << message << "\n";
  return 1;
}

prisminfer::KernelEvidence valid_evidence() {
  prisminfer::KernelEvidence evidence;
  evidence.schema_valid = true;
  evidence.correctness_passed = true;
  evidence.quality_fixture_present = true;
  evidence.quality_passed = true;
  evidence.same_cell_baseline = true;
  evidence.cap_accounted = true;
  evidence.cuda_involved = true;
  evidence.llama_ggml_cuda_baseline_present = true;
  evidence.workspace_peak_bytes = 1024;
  evidence.hard_cap_bytes = 2048;
  evidence.speedup_ratio = 1.11;
  return evidence;
}
}  // namespace

int main() {
  if (expect(prisminfer::validate_kernel_evidence(valid_evidence()).accepted,
             "valid kernel evidence accepted")) {
    return 1;
  }
  {
    auto evidence = valid_evidence();
    evidence.unknown_fields_present = true;
    const auto decision = prisminfer::validate_kernel_evidence(evidence);
    if (expect(!decision.accepted, "unknown fields rejected")) return 1;
    if (expect(decision.reason == "kernel_schema_required",
               "unknown field reason")) {
      return 1;
    }
  }
  {
    auto evidence = valid_evidence();
    evidence.full_dequant_materialized = true;
    if (expect(!prisminfer::validate_kernel_evidence(evidence).accepted,
               "full dequant rejected")) {
      return 1;
    }
  }
  {
    auto evidence = valid_evidence();
    evidence.cap_accounted = false;
    evidence.measured_non_certified = true;
    const auto decision = prisminfer::validate_kernel_evidence(evidence);
    if (expect(decision.accepted, "non-certified measured lane accepted")) {
      return 1;
    }
    if (expect(decision.status == "measured-non-certified",
               "non-certified status retained")) {
      return 1;
    }
  }
  {
    auto evidence = valid_evidence();
    evidence.speedup_ratio = 1.09;
    const auto decision = prisminfer::validate_kernel_evidence(evidence);
    if (expect(!decision.accepted, "slow kernel rejected")) return 1;
    if (expect(decision.reason == "kernel_not_faster_than_baseline",
               "slow kernel reason")) {
      return 1;
    }
  }
  return 0;
}

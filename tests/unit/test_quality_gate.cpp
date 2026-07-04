#include <iostream>

#include "prisminfer/kv_compression_policy.h"
#include "prisminfer/quality_gate.h"

namespace {

int expect(bool condition, const char* message) {
  if (condition) return 0;
  std::cerr << "FAIL: " << message << "\n";
  return 1;
}

}  // namespace

int main() {
  const auto skipped = prisminfer::evaluate_quality_gate(
      prisminfer::QualityGateInput{"none", 1.0, 1.0, 0.0, true, true});
  if (expect(skipped.passed && skipped.status == "skipped",
             "none gate skipped")) {
    return 1;
  }

  const auto passed = prisminfer::evaluate_quality_gate(
      prisminfer::QualityGateInput{"retrieval", 1.0, 1.01, 0.02, true, true});
  if (expect(passed.passed && passed.status == "passed",
             "retrieval gate passed")) {
    return 1;
  }

  const auto retrieval_failed = prisminfer::evaluate_quality_gate(
      prisminfer::QualityGateInput{"retrieval", 1.0, 1.0, 0.0, true, false});
  if (expect(!retrieval_failed.passed, "retrieval failure rejected")) return 1;
  if (expect(retrieval_failed.failure_reason == "retrieval_gate_failed",
             "retrieval failure reason")) {
    return 1;
  }

  const auto no_quality = prisminfer::evaluate_kv_compression_claim(
      prisminfer::KvCompressionInput{"reference", 1024, 512, 64, skipped});
  if (expect(!no_quality.accepted,
             "compression without quality gate rejected")) {
    return 1;
  }
  if (expect(no_quality.failure_reason ==
                 "quality_gate_required_for_compression",
             "compression quality reason")) {
    return 1;
  }

  const auto accepted = prisminfer::evaluate_kv_compression_claim(
      prisminfer::KvCompressionInput{"reference", 1024, 512, 64, passed});
  if (expect(accepted.accepted && accepted.saved_bytes == 448,
             "compression savings accepted")) {
    return 1;
  }
  return 0;
}

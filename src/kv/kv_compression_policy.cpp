#include "prisminfer/kv_compression_policy.h"

namespace prisminfer {

bool valid_kv_compression_policy(const std::string& policy) {
  return policy == "none" || policy == "accounting-only" ||
         policy == "reference" || policy == "experimental";
}

KvCompressionResult evaluate_kv_compression_claim(
    const KvCompressionInput& input) {
  if (!valid_kv_compression_policy(input.policy)) {
    return {false, "rejected", "kv_compression_policy_invalid", 0};
  }
  if (input.policy == "none" || input.policy == "accounting-only") {
    return {true, input.policy, "", 0};
  }
  if (input.quality.status == "skipped") {
    return {false, "rejected", "quality_gate_required_for_compression", 0};
  }
  if (!input.quality.passed) {
    return {false, "rejected", input.quality.failure_reason, 0};
  }
  const std::uint64_t compressed_total =
      input.compressed_bytes + input.metadata_bytes;
  if (compressed_total >= input.baseline_bytes) {
    return {false, "rejected", "kv_compression_no_memory_savings", 0};
  }
  return {true, "accepted", "", input.baseline_bytes - compressed_total};
}

}  // namespace prisminfer

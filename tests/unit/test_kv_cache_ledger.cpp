#include <iostream>

#include "prisminfer/kv_cache_ledger.h"

namespace {

int expect(bool condition, const char* message) {
  if (condition) return 0;
  std::cerr << "FAIL: " << message << "\n";
  return 1;
}

}  // namespace

int main() {
  prisminfer::KvCacheProfile profile;
  const auto profile_result =
      prisminfer::make_kv_cache_profile(2, 4, 8, 16, 16, 16, &profile);
  if (expect(profile_result.ok, "valid profile accepted")) return 1;
  if (expect(profile.bytes_per_token == 256, "bytes per token computed")) {
    return 1;
  }
  if (expect(profile.bytes_per_block == 4096, "bytes per block computed")) {
    return 1;
  }

  prisminfer::KvCacheLedger ledger;
  if (expect(ledger.record_profile(profile).ok, "profile recorded")) return 1;
  prisminfer::KvCacheSample sample;
  sample.logical_tokens = 32;
  sample.active_blocks = 2;
  sample.gpu_bytes = 8192;
  sample.metadata_bytes = 128;
  sample.evidence_status = "reconciled";
  if (expect(ledger.sample(sample, 256).ok, "sample under budget accepted")) {
    return 1;
  }
  if (expect(ledger.peak_sample().gpu_bytes == 8192,
             "peak sample retained")) {
    return 1;
  }

  sample.metadata_bytes = 512;
  const auto budget = ledger.sample(sample, 256);
  if (expect(!budget.ok, "metadata budget breach rejected")) return 1;
  if (expect(budget.failure_reason == "kv_metadata_budget_exceeded",
             "metadata budget failure reason")) {
    return 1;
  }

  const auto invalid =
      prisminfer::make_kv_cache_profile(0, 4, 8, 16, 16, 16, &profile);
  if (expect(!invalid.ok, "invalid profile rejected")) return 1;
  if (expect(invalid.failure_reason == "kv_profile_metadata_incomplete",
             "invalid profile reason")) {
    return 1;
  }
  return 0;
}

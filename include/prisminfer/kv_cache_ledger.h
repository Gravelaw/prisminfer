#pragma once

#include <cstdint>
#include <string>

namespace prisminfer {

struct KvCacheProfile {
  std::uint32_t layer_count{0};
  std::uint32_t kv_head_count{0};
  std::uint32_t head_dim{0};
  std::uint32_t block_tokens{16};
  std::uint32_t key_bits{16};
  std::uint32_t value_bits{16};
  std::uint64_t bytes_per_token{0};
  std::uint64_t bytes_per_block{0};
};

struct KvCacheSample {
  std::uint64_t logical_tokens{0};
  std::uint64_t active_blocks{0};
  std::uint64_t reusable_blocks{0};
  std::uint64_t evicted_blocks{0};
  std::uint64_t gpu_bytes{0};
  std::uint64_t host_bytes{0};
  std::uint64_t compressed_bytes{0};
  std::uint64_t metadata_bytes{0};
  std::uint64_t unknown_bytes{0};
  std::string evidence_status{"estimated"};
};

struct KvCacheLedgerResult {
  bool ok{false};
  std::string failure_reason;
};

KvCacheLedgerResult make_kv_cache_profile(std::uint32_t layer_count,
                                          std::uint32_t kv_head_count,
                                          std::uint32_t head_dim,
                                          std::uint32_t block_tokens,
                                          std::uint32_t key_bits,
                                          std::uint32_t value_bits,
                                          KvCacheProfile* profile);

class KvCacheLedger {
 public:
  KvCacheLedgerResult record_profile(const KvCacheProfile& profile);
  KvCacheLedgerResult sample(const KvCacheSample& sample,
                             std::uint64_t metadata_budget_bytes);

  const KvCacheProfile& profile() const;
  const KvCacheSample& peak_sample() const;
  bool has_profile() const;

 private:
  KvCacheProfile profile_;
  KvCacheSample peak_sample_;
  bool has_profile_{false};
};

}  // namespace prisminfer
